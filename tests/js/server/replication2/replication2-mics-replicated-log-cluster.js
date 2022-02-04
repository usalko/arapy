/*jshint strict: true */
/*global assertTrue, assertEqual*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const {sleep} = require('internal');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const LH = require("@arangodb/testutils/replicated-logs-helper");

const database = "replication2_misc_test_db";

const waitForReplicatedLogAvailable = function (id) {
  while (true) {
    try {
      let status = db._replicatedLog(id).status();
      const leaderId = status.leaderId;
      if (leaderId !== undefined && status.participants !== undefined &&
          status.participants[leaderId].role === "leader") {
        break;
      }
      console.info("replicated log not yet available");
    } catch (err) {
      const errors = [
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code,
        ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code
      ];
      if (errors.indexOf(err.errorNum) === -1) {
        throw err;
      }
    }

    sleep(1);
  }
};


const miscReplicatedLogSuite = function () {

  const targetConfig = {
    writeConcern: 2,
    softWriteConcern: 2,
    replicationFactor: 3,
    waitForSync: false,
  };

  const {setUpAll, tearDownAll, stopServer, continueServer, resumeAll} = (function () {
    let previousDatabase, databaseExisted = true;
    let stoppedServers = {};
    return {
      setUpAll: function () {
        previousDatabase = db._name();
        if (!_.includes(db._databases(), database)) {
          db._createDatabase(database);
          databaseExisted = false;
        }
        db._useDatabase(database);
      },

      tearDownAll: function () {
        db._useDatabase(previousDatabase);
        if (!databaseExisted) {
          db._dropDatabase(database);
        }
      },
      stopServer: function (serverId) {
        if (stoppedServers[serverId] !== undefined) {
          throw Error(`${serverId} already stopped`);
        }
        LH.stopServer(serverId);
        stoppedServers[serverId] = true;
      },
      continueServer: function (serverId) {
        if (stoppedServers[serverId] === undefined) {
          throw Error(`${serverId} not stopped`);
        }
        LH.continueServer(serverId);
        delete stoppedServers[serverId];
      },
      resumeAll: function () {
        Object.keys(stoppedServers).forEach(function (key) {
          continueServer(key);
        });
      }
    };
  }());

  const getReplicatedLogLeaderPlan = function (database, logId) {
    let {plan} = LH.readReplicatedLogAgency(database, logId);
    if (!plan.currentTerm) {
      throw Error("no current term in plan");
    }
    if (!plan.currentTerm.leader) {
      throw Error("current term has no leader");
    }
    const leader = plan.currentTerm.leader.serverId;
    const term = plan.currentTerm.term;
    return {leader, term};
  };

  const createReplicatedLogAndWaitForLeader = function (database) {
    const logId = LH.nextUniqueLogId();
    const servers = _.sampleSize(LH.dbservers, targetConfig.replicationFactor);
    LH.replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: LH.getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
    });

    LH.waitFor(LH.replicatedLogLeaderEstablished(database, logId, undefined, servers));

    const {leader, term} = getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  };

  const getSupervisionActionTypes = function (database, logId) {
    const {current} = LH.readReplicatedLogAgency(database, logId);
    // filter out all empty actions
    const actions = _.filter(current.supervision.actions, (a) => a.desc.type !== "EmptyAction");
    return _.map(actions, (a) => a.desc.type);
  };

  const getLastNSupervisionActionsType = function (database, logId, n) {
    const actions = getSupervisionActionTypes(database, logId);
    return _.takeRight(actions, n);
  };

  const countActionsByType = function (actions) {
    return _.reduce(actions, function (acc, type) {
      acc[type] = 1 + (acc[type] || 0);
      return acc;
    }, {});
  };

  return {
    setUpAll, tearDownAll,
    setUp: LH.registerAgencyTestBegin,
    tearDown: function (test) {
      resumeAll();
      LH.waitFor(LH.allServersHealthy());
      LH.registerAgencyTestEnd(test);
    },

    testUpdateTargetWithParticipants: function () {
      const logId = LH.nextUniqueLogId();
      LH.replicatedLogSetTarget(database, logId, {
        id: logId,
        config: targetConfig,
        supervision: {maxActionsTraceLength: 20},
      });

      LH.waitFor(function () {
        const {target} = LH.readReplicatedLogAgency(database, logId);
        if (target.participants === undefined) {
          return Error("Target/Participants not yet set");
        }
        return true;
      });

      {
        const {target} = LH.readReplicatedLogAgency(database, logId);
        assertEqual(targetConfig.replicationFactor, _.size(target.participants));
      }
    },

    testReplaceMultipleFollowers: function () {
      const {servers, leader, followers, logId, term} = createReplicatedLogAndWaitForLeader(database);
      const otherServer = _.difference(LH.dbservers, servers);

      const newFollowers = _.sampleSize(otherServer, 2);
      LH.updateReplicatedLogTarget(database, logId, function (target) {
        delete target.participants[followers[0]];
        delete target.participants[followers[1]];
        target.participants[newFollowers[0]] = {};
        target.participants[newFollowers[1]] = {};
      });

      LH.waitFor(LH.replicatedLogParticipantsFlag(database, logId, {
        [followers[0]]: null,
        [followers[1]]: null,
        [newFollowers[0]]: {excluded: false, forced: false},
        [newFollowers[1]]: {excluded: false, forced: false},
      }));
      LH.waitFor(LH.replicatedLogIsReady(database, logId, term, [leader, ...newFollowers], leader));

      {
        const actions = getLastNSupervisionActionsType(database, logId, 4);
        const expectedActions = [
          "AddParticipantToPlanAction",
          "RemoveParticipantFromPlan",
          "AddParticipantToPlanAction",
          "RemoveParticipantFromPlan",
        ];
        assertEqual(actions, expectedActions);
      }

      LH.replicatedLogDeleteTarget(database, logId);
    },

    testReplaceLeaderAndOneFollower: function () {
      const {servers, leader, followers, logId, term} = createReplicatedLogAndWaitForLeader(database);

      const initialNumberOfActions = getSupervisionActionTypes(database, logId).length;

      const otherServer = _.difference(LH.dbservers, servers);
      const [newLeader, newFollower] = _.sampleSize(otherServer, 2);
      const oldFollower = followers[0];
      LH.updateReplicatedLogTarget(database, logId, function (target) {
        delete target.participants[oldFollower];
        delete target.participants[leader];
        target.participants[newLeader] = {};
        target.participants[newFollower] = {};
        target.leader = newLeader;
      });

      LH.waitFor(LH.replicatedLogParticipantsFlag(database, logId, {
        [oldFollower]: null,
        [leader]: null,
        [newLeader]: {excluded: false, forced: false},
        [newFollower]: {excluded: false, forced: false},
        [followers[1]]: {excluded: false, forced: false},
      }));
      LH.waitFor(LH.replicatedLogIsReady(database, logId, term + 1, [newLeader, newFollower, followers[1]], newLeader));

      const actions = getSupervisionActionTypes(database, logId);
      const actionsRequired = actions.length - initialNumberOfActions;
      const last = _.takeRight(actions, actionsRequired);
      const counts = countActionsByType(last);
      const expected = {
        "RemoveParticipantFromPlan": 2, // we removed two participants
        "AddParticipantToPlanAction": 2, // and added two other participants
        "DictateLeaderAction": 1, // single dictate leader - add newLeader, remove oldFollower, Failover to newLeader, add newFollower, remove leader
        "UpdateParticipantFlags": 2 // each dictate leadership comes with two UpdateParticipantFlags action
      };
      assertEqual(counts, expected);

      LH.replicatedLogDeleteTarget(database, logId);
    },

    testReplaceAllServers: function () {
      const {servers, leader, followers, logId, term} = createReplicatedLogAndWaitForLeader(database);

      const initialNumberOfActions = getSupervisionActionTypes(database, logId).length;

      const otherServer = _.difference(LH.dbservers, servers);
      const [newLeader, ...newFollowers] = _.sampleSize(otherServer, 2);
      LH.updateReplicatedLogTarget(database, logId, function (target) {
        delete target.participants[followers[0]];
        delete target.participants[followers[1]];
        delete target.participants[leader];
        target.participants[newLeader] = {};
        target.participants[newFollowers[0]] = {};
        target.participants[newFollowers[1]] = {};
        target.leader = newLeader;
      });

      LH.waitFor(LH.replicatedLogParticipantsFlag(database, logId, {
        [followers[0]]: null,
        [followers[1]]: null,
        [leader]: null,
        [newLeader]: {excluded: false, forced: false},
        [newFollowers[0]]: {excluded: false, forced: false},
        [newFollowers[1]]: {excluded: false, forced: false},
      }));
      LH.waitFor(LH.replicatedLogIsReady(database, logId, term + 1, otherServer, newLeader));

      const actions = getSupervisionActionTypes(database, logId);
      const actionsRequired = actions.length - initialNumberOfActions;
      const last = _.takeRight(actions, actionsRequired);
      const counts = countActionsByType(last);
      const expected = {
        "RemoveParticipantFromPlan": 3, // we removed two participants
        "AddParticipantToPlanAction": 3, // and added two other participants
        "DictateLeaderAction": 1, // single dictate leader - add newLeader, remove oldFollower, Failover to newLeader, add newFollower, remove leader
        "UpdateParticipantFlags": 2 // each dictate leadership comes with two UpdateParticipantFlags action
      };
      assertEqual(counts, expected);

      LH.replicatedLogDeleteTarget(database, logId);
    }
  };
};

jsunity.run(miscReplicatedLogSuite);
return jsunity.done();

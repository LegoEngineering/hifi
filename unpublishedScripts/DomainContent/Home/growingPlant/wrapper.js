//
//  growingPlantSpawner.js
//  examples/homeContent/plant
//
//  Created by Eric Levin on 2/10/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  This entity script handles the logic for growing a plant when it has water poured on it
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
var PLANT_SCRIPT_URL = Script.resolvePath("atp:/scripts/growingPlantEntityScript.js?v1" + Math.random());
var WATER_CAN_SCRIPT_URL = Script.resolvePath("atp:/scripts/waterCanEntityScript.js?v1" + Math.random());
Plant = function(spawnPosition, spawnRotation) {
  var orientation;
  if (spawnRotation !== undefined) {
    orientation = Quat.fromPitchYawRollDegrees(spawnRotation.x, spawnRotation.y, spawnRotation.z);
  } else {
    orientation = Camera.getOrientation();
  }
  print("EBL ORIENTATION " + JSON.stringify(orientation));
  var bowlPosition = spawnPosition;
  var BOWL_MODEL_URL = "atp:/models/Flowers-Bowl.fbx";
  var bowlDimensions = {
    x: 0.518,
    y: 0.1938,
    z: 0.5518
  };
  var bowl = Entities.addEntity({
    type: "Model",
    modelURL: BOWL_MODEL_URL,
    dimensions: bowlDimensions,
    name: "plant bowl",
    position: bowlPosition,
    userData: JSON.stringify({
      'hifiHomeKey': {
        'reset': true
      }
    }),
  });


  var PLANT_MODEL_URL = "atp:/models/Flowers-Rock.fbx";

  var plantDimensions = {
    x: 0.52,
    y: 0.2600,
    z: 0.52
  };
  var plantPosition = Vec3.sum(bowlPosition, {
    x: 0,
    y: plantDimensions.y / 2,
    z: 0
  });
  var plant = Entities.addEntity({
    type: "Model",
    modelURL: PLANT_MODEL_URL,
    name: "hifi-growable-plant",
    dimensions: plantDimensions,
    position: plantPosition,
    script: PLANT_SCRIPT_URL,
    parentID: bowl,
    userData: JSON.stringify({
      'hifiHomeKey': {
        'reset': true
      }
    }),
  });


  var WATER_CAN_MODEL_URL = "atp:/models/waterCan.fbx";

  var waterCanPosition = Vec3.sum(plantPosition, Vec3.multiply(0.6, Quat.getRight(orientation)));
  var waterCanRotation = orientation;
  var waterCan = Entities.addEntity({
    type: "Model",
    shapeType: 'box',
    name: "hifi-water-can",
    modelURL: WATER_CAN_MODEL_URL,
    script: WATER_CAN_SCRIPT_URL,
    dimensions: {
      x: 0.1859,
      y: 0.2762,
      z: 0.4115
    },
    position: waterCanPosition,
    collisionSoundURL: "atp:/sounds/watering_can_drop.L.wav",
    angularDamping: 1,
    dynamic: true,
    gravity: {
      x: 0.0,
      y: -2.0,
      z: 0
    },
    velocity: {x: -0, y: -0.2, z: 0},
    rotation: waterCanRotation,
    userData: JSON.stringify({
      'hifiHomeKey': {
        'reset': true
      },
      wearable: {
        joints: {
          RightHand: [{
            x: 0.024,
            y: 0.173,
            z: 0.152
          }, {
            x: 0.374,
            y: 0.636,
            z: -0.638,
            w: -0.215
          }],
          LeftHand: [{
            x: -0.0348,
            y: 0.201,
            z: 0.166
          }, {
            x: 0.4095,
            y: -0.625,
            z: 0.616,
            w: -0.247
          }]
        }
      }
    })
  });


  var waterSpoutPosition = Vec3.sum(waterCanPosition, Vec3.multiply(0.2, Quat.getFront(orientation)))
  var waterSpoutRotation = Quat.multiply(waterCanRotation, Quat.fromPitchYawRollDegrees(10, 0, 0));
  var waterSpout = Entities.addEntity({
    type: "Box",
    name: "hifi-water-spout",
    dimensions: {
      x: 0.02,
      y: 0.02,
      z: 0.07
    },
    color: {
      red: 200,
      green: 10,
      blue: 200
    },
    position: waterSpoutPosition,
    rotation: waterSpoutRotation,
    parentID: waterCan,
    visible: false,
    userData: JSON.stringify({
      'hifiHomeKey': {
        'reset': true
      }
    }),
  });

  function cleanup() {
    print('PLANT CLEANUP!')
    Entities.deleteEntity(plant);
    Entities.deleteEntity(bowl);
    Entities.deleteEntity(waterCan);
    Entities.deleteEntity(waterSpout);
  }

  this.cleanup = cleanup;

  print('CREATED PLANT:: ' + plant)


}
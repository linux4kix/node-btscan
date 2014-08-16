var BtScan = require('../');
var btscan = new BtScan();

btscan.on('stateChange', function(state) {
  if (state === 'poweredOn') {
    btscan.startScanning();
  } else {
    btscan.stopScanning();
  }
});

btscan.on('discover', function(device) {
  console.log('device discovered (' + device.address+ '):');
});

/*
process.on('SIGINT', function() {
  console.log("process exited");
  btscan.stopScanning();
});
*/

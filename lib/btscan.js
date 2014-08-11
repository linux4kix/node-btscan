var events = require('events');
var spawn = require('child_process').spawn;
var util = require('util');

var BtScan = function() {
  var hciBle = __dirname + '/../build/Release/hci-btscan';
  
  // console.log('hciBle = ' + hciBle);

  this._btScan = spawn(hciBle);
//  this._btScan.on('close', this.onClose.bind(this));

  this._btScan.stdout.on('data', this.onStdoutData.bind(this));
  this._btScan.stderr.on('data', this.onStderrData.bind(this));

  this._btScan.on('error', function() { });

  this._buffer = "";

  this._discoveries = {};
};

util.inherits(BtScan, events.EventEmitter);

BtScan.prototype.onClose = function(code) {
  console.log('close = ' + code);
};

BtScan.prototype.onStdoutData = function(data) {
  this._buffer += data.toString();

  // console.log('buffer = ' + JSON.stringify(this._buffer));

  var newLineIndex;
  while ((newLineIndex = this._buffer.indexOf('\n')) !== -1) {
    var line = this._buffer.substring(0, newLineIndex);
    var found;
    
    this._buffer = this._buffer.substring(newLineIndex + 1);

    // console.log('line = ' + line);

    if ((found = line.match(/^adapterState (.*)$/))) {
      var adapterState = found[1];

      // console.log('adapterState = ' + adapterState);

      if (adapterState === 'unauthorized') {
        console.log('btscan warning: adapter state unauthorized, please run as root or with sudo');
      }
      
      this.emit('stateChange', adapterState);
    } else if ((found = line.match(/^result (.*)$/))) {
      var result = found[1];
      var splitResult = result.split(',');

      var address = splitResult[0];
      var rssi = parseInt(splitResult[1], 10);

      // console.log('address = ' + address);
      // console.log('rssi = ' + rssi);

      var previouslyDiscovered = !!this._discoveries[address];

      var discoveryCount = previouslyDiscovered ? this._discoveries[address].count : 0;

      if (discoveryCount % 2 === 0) {
        // reset service data every second event
      }

      this._discoveries[address] = {
        address: address,
        rssi: rssi,
        count: (discoveryCount + 1)
      };

      if (this._discoveries[address].count == 2) {
        this.emit('discover', {'address': address,'rssi': rssi});
      }
    }
  }
};

BtScan.prototype.onStderrData = function(data) {
  console.error('stderr: ' + data);
};

BtScan.prototype.startScanning = function(allowDuplicates) {
  this._btScan.kill(allowDuplicates ? 'SIGUSR2' : 'SIGUSR1');

  this.emit('scanStart');
};

BtScan.prototype.stopScanning = function() {
  this._btScan.kill('SIGHUP');

  this.emit('scanStop');
};

module.exports = BtScan;


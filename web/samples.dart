var sampleConfigurations = {
  'Empty' : [],
  'Simplest' : simplest,
  'Triple-Click' : tripleClick,
};

var simplest = [{
  'brightness': '100',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'frequency': '10',
  'morse': 'SOS',
  'morseCpm': '10',
  'conds': [{'type':'Push', 'to':0, 'time':'1'}]
}];

var tripleClick = [{
  'brightness': '25',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'frequency': '10',
  'morse': 'SOS',
  'morseCpm': '10',
  'conds': [{'type':'Push', 'to':2, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'1'}]
}, {
  'brightness': '100',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'frequency': '10',
  'morse': 'SOS',
  'morseCpm': '10',
  'conds': [{'type':'Push', 'to':3, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'1'}]
}, {
  'brightness': '100',
  'highPower': true,
  'action': 'On',
  'dutyCycle': '10',
  'frequency': '10',
  'morse': 'SOS',
  'morseCpm': '10',
  'conds': [{'type':'Push', 'to':1, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'1'}]
}, {
  'brightness': '100',
  'highPower': false,
  'action': 'Unchanged',
  'dutyCycle': '10',
  'frequency': '10',
  'morse': 'SOS',
  'morseCpm': '10',
  'conds': [{'type':'Push', 'to':0, 'time':'1'}]
}];


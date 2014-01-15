var sampleConfigurations = {
  'Empty' : [],
  'Simple toggle' : simplest,
  'Triple-Click' : tripleClick,
};

var simplest = [{
  'brightness': '127',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'period': '10',
  'conds': [{'type':'Push', 'to':0, 'time':'1'}]
}];

var tripleClick = [{
  'brightness': '32',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'period': '10',
  'conds': [{'type':'Push', 'to':2, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'500'}]
}, {
  'brightness': '127',
  'highPower': false,
  'action': 'On',
  'dutyCycle': '10',
  'period': '10',
  'conds': [{'type':'Push', 'to':3, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'500'}]
}, {
  'brightness': '127',
  'highPower': true,
  'action': 'On',
  'dutyCycle': '10',
  'period': '10',
  'conds': [{'type':'Push', 'to':1, 'time':'1'},
            {'type':'Idle', 'to':4, 'time':'500'}]
}, {
  'brightness': '127',
  'highPower': false,
  'action': 'Unchanged',
  'dutyCycle': '10',
  'period': '10',
  'conds': [{'type':'Push', 'to':0, 'time':'1'}]
}];


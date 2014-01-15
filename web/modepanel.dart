import 'package:polymer/polymer.dart';
import 'package:polymer_expressions/filter.dart';
import 'dart:html';
import 'samples.dart';

@CustomTag('mode-panel')
class ModePanel extends PolymerElement {
  static const version = 1;
  
  @observable ObservableList modes;
  @observable String outputCode = "";
  var samples = toObservable(sampleConfigurations);
  final Transformer indGen = new GenerateIterable();

  ModePanel.created() : super.created() {
    modes = new ObservableList();
    modes.changes.listen((T) => handleChanges());
    
  }
  
  void addNewMode() {
    var mode = toObservable({
     'brightness': '127',
     'highPower': false,
     'action': 'On',
     'dutyCycle': '10',
     'period': '10',
     'conds': [],
   });
    
    mode.changes.listen((T) => handleChanges());
    mode['conds'].changes.listen((T) => handleChanges());
    modes.add(mode);
  }
  
  void addNewCondition(ObservableMap mode) {
    var condition = toObservable({'type':'Push', 'to':0, 'time':'1'});
    condition.changes.listen((T) => handleChanges());
    mode['conds'].add(condition);
  }
  
  void addMode(Event e, var detail, Element target) {
    addNewMode();
  }
  
  void delMode(Event e, var detail, Element target) {
    var modeno = int.parse(target.dataset['modeno']);
    modes.removeAt(modeno);
  }
  
  void addCondition(Event e, var detail, Element target) {
    var modeno = int.parse(target.dataset['modeno']);
    addNewCondition(modes[modeno]);
  }
  
  void delCondition(Event e, var detail, Element target) {
    var modeno = int.parse(target.dataset['modeno']);
    var condno = int.parse(target.dataset['condno']);
    modes[modeno]['conds'].removeAt(condno);
  }

  void loadSample(Event e, var detail, Element target) {
    var key = target.dataset['sample'];
    modes.clear();
    modes.addAll(samples[key]);
    for (var mode in modes) {
      mode.changes.listen((T) => handleChanges());
      for (var cond in mode['conds'])
        cond.changes.listen((T) => handleChanges());
    }
  }
  
  String byteToPercent(String byte) {
    var val = int.parse(byte);
    return (val * 100 ~/ 127).toString();
  }
  
  String packConfiguration(var conf) {
    String intToHex(int val, int chars) {
      String result = val.toRadixString(16);
      while (result.length < chars) 
        result = '0' + result;
      return result;
    }

    var result = new StringBuffer('[');
    result.write(intToHex(version, 2));
    result.write(':');
    
    bool first = true;
    for (var mode in modes) {
      if (first) 
        first = false;
      else
        result.write(',');
      
      int brightness = int.parse(mode['brightness']);
      if (mode['highPower']) brightness |= 0x80;
      int period     = int.parse(mode['period']);
      int dutyCycle  = int.parse(mode['dutyCycle']);
      
      switch (mode['action']) {
        case 'On':
          result.write('O');
          result.write(intToHex(brightness, 2));
          break;
        case 'Flash':
          result.write('F');
          result.write(intToHex(brightness, 2));
          result.write(intToHex(period, 4));
          result.write(intToHex(dutyCycle, 2));
          break;
        case 'Dazzle':
          result.write('D');
          result.write(intToHex(brightness, 2));
          result.write(intToHex(period, 4));
          result.write(intToHex(dutyCycle, 2));
          break;
        case 'Unchanged':
          result.write('U');
          break;
      }
      
      for (var cond in mode['conds']) {
        int toMode = cond['to'];//int.parse(cond['to']);
        int time   = int.parse(cond['time']);
        
        switch (cond['type']) {
          case 'Push':
            result.write('p');
            result.write(intToHex(toMode, 2));
            break;
          case 'Hold':
            result.write('h');
            result.write(intToHex(toMode, 2));
            result.write(intToHex(time, 4));
            break;
          case 'Release':
            result.write('r');
            result.write(intToHex(toMode, 2));
            break;
          case 'Idle':
            result.write('i');
            result.write(intToHex(toMode, 2));
            result.write(intToHex(time, 4));
            break;
          case 'Tap':
            result.write('t');
            result.write(intToHex(toMode, 2));
            break;
        }
      }
    }
    
    result.write(']');
    
    return result.toString();
  }
  
  void handleChanges() {
    outputCode = packConfiguration(modes);
  }
}

class GenerateIterable extends Transformer<Iterable, int> {
  int reverse(Iterable i) => i.length;
  Iterable forward(int i) => new Iterable.generate(i, (j) => j.toString());
}

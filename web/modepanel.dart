import 'package:polymer/polymer.dart';
import 'package:polymer_expressions/filter.dart';
import 'dart:html';
import 'samples.dart';

@CustomTag('mode-panel')
class ModePanel extends PolymerElement {
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
     'brightness': '100',
     'highPower': false,
     'action': 'On',
     'dutyCycle': '10',
     'frequency': '10',
     'morse': 'SOS',
     'morseCpm': '10',
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
  
  String packConfiguration(var conf) {
    var result = new StringBuffer();
    
    String intToHex(int val, int chars) {
      String result = val.toRadixString(16);
      while (result.length < chars) 
        result = '0' + result;
      return result;
    }
    
    for (var mode in modes) {
      int brightness = int.parse(mode['brightness']);
      if (mode['highPower']) brightness |= 0x80;
      int frequency  = int.parse(mode['frequency']);
      int dutyCycle  = int.parse(mode['dutyCycle']);
      
      switch (mode['action']) {
        case 'On':
          result.write('o');
          result.write(intToHex(brightness, 2));
          break;
        case 'Flash':
          result.write('f');
          result.write(intToHex(brightness, 2));
          result.write(intToHex(frequency, 4));
          result.write(intToHex(dutyCycle, 2));
          break;
        case 'Dazzle':
          result.write('d');
          result.write(intToHex(brightness, 2));
          result.write(intToHex(frequency, 4));
          result.write(intToHex(dutyCycle, 2));
          break;
        case 'Morse':
          result.write('m');
          result.write(intToHex(brightness, 2));
          result.write(intToHex(mode['morse'].length, 2));
          result.write(mode['morse']);
          result.write(intToHex(int.parse(mode['morseCpm']), 2));
          break;
        case 'Unchanged':
          result.write('u');
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
          case 'Unhold':
            result.write('u');
            result.write(intToHex(toMode, 2));
            break;
          case 'Idle':
            result.write('i');
            result.write(intToHex(toMode, 2));
            result.write(intToHex(time, 4));
            break;
        }
      }
      
      result.write('.');
    }
    
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
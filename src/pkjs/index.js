var Clay = require('@rebble/clay');
var clayConfig = require('./Config.js');
var clay = new Clay(clayConfig);

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude;
      var lon = pos.coords.longitude;
      var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat + '&longitude=' + lon + '&current_weather=true&daily=weathercode&timezone=auto';

      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.onload = function() {
        if (req.readyState === 4 && req.status === 200) {
          var response = JSON.parse(req.responseText);
          var currentCode = response.current_weather.weathercode;
          var tomorrowCode = response.daily.weathercode[1];

          function mapCodeToEnum(code) {
            if (code === 0) return 0;
            if (code === 1 || code === 2 || code === 3 || code === 45 || code === 48) return 1;
            if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return 2;
            if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return 3;
            if (code >= 95 && code <= 99) return 4;
            return 0;
          }

          Pebble.sendAppMessage({
            'WeatherStatus': mapCodeToEnum(currentCode),
            'WeatherStatusTomorrow': mapCodeToEnum(tomorrowCode)
          });
        }
      };
      req.send(null);
    },
    function(err) { console.log('Error requesting location!'); },
    { timeout: 15000, maximumAge: 60000 }
  );
}

Pebble.addEventListener('ready', function(e) {
  console.log('PebbleKit JS ready!');
  // 💡 NEW：スマホと時計が繋がった瞬間に、「今の設定データを全部教えて！」と合図を送る
  Pebble.sendAppMessage({ 'RequestSettings': 1 });
});

Pebble.addEventListener('appmessage', function(e) {
  var dict = e.payload;

  // 💡 NEW：時計から「設定データ（Themeなど）」が大量に送られてきた場合の処理
  if (dict['Theme'] !== undefined) {
    console.log('Settings received from watch!');
    
    var claySettings = {};
    for (var key in dict) {
      // "RequestSettings" など設定画面に不要な通信キー以外を保存する
      if (key !== 'RequestSettings') {
        claySettings[key] = dict[key];
      }
    }
    
    // 👑 究極の魔法：Clayが読み込むスマホ側の保存領域（localStorage）を、時計のデータで強制上書き！
    localStorage.setItem('clay-settings', JSON.stringify(claySettings));
  } 
  // 💡 従来通り：設定データではない通信（毎時0分の合図など）が来た場合は天気を更新！
  else {
    fetchWeather();
  }
});
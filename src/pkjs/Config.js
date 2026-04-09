module.exports = [
  {
    "type": "heading",
    "defaultValue": "Iris Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize your mechanical shutter watchface."
  },
  
  // ==========================================
  // 1. 見た目の設定 (Appearance)
  // ==========================================
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Appearance"
      },
      {
        "type": "select",
        "messageKey": "WatchFaceMode",
        "label": "Appearance Mode",
        "defaultValue": "0",
        "options": [
          { "label": "Digital", "value": "0" },
          { "label": "Analog", "value": "1" }
        ]
      },
			      {
        "type": "select",
        "messageKey": "ShutterThickness",
        "label": "Aperture Size",
        "defaultValue": "1",
        "options": [
          { "label": "Wide", "value": "0" },
          { "label": "Medium", "value": "1" },
          { "label": "Narrow", "value": "2" }
        ]
      },
      {
        "type": "select",
        "messageKey": "Theme",
        "label": "Background Color",
        "defaultValue": "0",
        "options": [
          { "label": "White", "value": "0" },
          { "label": "Black", "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ShutterColorMode",
        "label": "Blade Color Style",
        "defaultValue": "4", 
        "capabilities": ["COLOR"],
        "options": [
          { "label": "Solid Black", "value": "0" },
          { "label": "Solid White", "value": "1" },
          { "label": "Custom Solid Color", "value": "2" },
          { "label": "Vivid Rainbow", "value": "3" },
          { "label": "Pastel Rainbow", "value": "4" },
          { "label": "Battery Sync Vivid", "value": "5" },
          { "label": "Battery Sync Pastel", "value": "6" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ShutterColorMode",
        "label": "Blade Color Style",
        "defaultValue": "0", 
        "capabilities": ["BW"], 
        "options": [
          { "label": "Solid Black", "value": "0" },
          { "label": "Solid White", "value": "1" }
        ]
      },
      {
        "type": "color",
        "messageKey": "CustomColor",
        "label": "Custom Blade Color",
        "description": "Used only if 'Custom Solid Color' is selected above.",
        "defaultValue": "00FFFF",
        "allowFading": false,
        "capabilities": ["COLOR"]
      },
      {
        "type": "toggle",
        "messageKey": "BatteryColorSync",
        "label": "Drop Blades on Low Battery",
        "description": "Blades will drop when battery is under 50%.",
        "defaultValue": true
      }
    ]	
  },

  // ==========================================
  // 2. 動きと音 (Animation & Alerts)
  // ==========================================
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Animation & Vibration"
      },
      {
        "type": "slider",
        "messageKey": "AnimSpeed",
        "label": "Animation Duration (Larger = Slower)",
        "defaultValue": 40,
        "min": 10,
        "max": 100,
        "step": 10
      },
      {
        "type": "slider",
        "messageKey": "AnimFreq",
        "label": "Auto-Animation Interval (Minutes)",
        "description": "Set to 0 to disable automatic animations.",
        "defaultValue": 1,
        "min": 0,
        "max": 60,
        "step": 1
      },
      {
        "type": "select",
        "messageKey": "ChimeInterval",
        "label": "Periodic Vibration Interval",
        "description": "Set the interval for periodic vibration alerts.",
        "defaultValue": "60",
        "options": [
          { "label": "Every 5 Minutes", "value": "5" },
          { "label": "Every 10 Minutes", "value": "10" },
          { "label": "Every 15 Minutes", "value": "15" },
          { "label": "Every 20 Minutes", "value": "20" },
          { "label": "Every 30 Minutes", "value": "30" },
          { "label": "Every 1 Hour", "value": "60" }
        ]
      },
      {
        "type": "select",
        "messageKey": "VibeMode",
        "label": "Vibration Mode",
				"description": "Vibrations are paused during Quiet Time so you won't be disturbed.",
        "defaultValue": "2",
        "options": [
          { "label": "Shake & Periodic Vibration", "value": "0" },
          { "label": "Periodic Vibration Only", "value": "1" },
          { "label": "Shake Only", "value": "2" },
          { "label": "Disabled", "value": "3" }
        ]
      }
    ]
  },
	
	{
    "type": "submit",
    "defaultValue": "SAVE SETTINGS"
	  },
	
  // ==========================================
  // 3. ビジネスタイム (Business Time)
  // ==========================================
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "🏢 Business Time"
      },
{
        "type": "toggle",
        "messageKey": "EnableBusiness",
        "label": "Enable Business Time", // 💡 スッキリ！
        "defaultValue": false,
        "description": "When Quiet Time is active during your set hours, the watch automatically switches to a professional look, pauses regular animations, and uses business vibration settings. Outside these hours, it enters a muted Sleep Mode." // 💡 Chimeを消去＆UX向上！
      },
			
      {
        "type": "select",
        "messageKey": "BizDays",
        "label": "Active Days",
        "defaultValue": "1",
        "options": [
          { "label": "Everyday", "value": "0" },
          { "label": "Weekdays (Mon-Fri)", "value": "1" }
        ]
      },
      {
        "type": "slider",
        "messageKey": "BizStartH",
        "defaultValue": 9,
        "label": "Business Start Time (Hour: 0-23)",
        "description": "If you're a superhuman working 24/7, set the Start and End to the exact same time!",
        "min": 0,
        "max": 23,
        "step": 1
      },
      {
        "type": "slider",
        "messageKey": "BizEndH",
        "defaultValue": 18,
        "label": "Business End Time (Hour: 0-23)",
				"description": "Tip: Set End Time smaller than Start Time (e.g., 22 to 2) for overnight shifts.",
        "min": 0,
        "max": 23,
        "step": 1
      },
      {
        "type": "select",
        "messageKey": "BizFace",
        "label": "Appearance Mode",
        "defaultValue": "1",
        "options": [
          { "label": "Digital", "value": "0" },
          { "label": "Analog", "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "BizShutter",
        "label": "Aperture Size",
        "defaultValue": "0",
        "options": [
          { "label": "Wide", "value": "0" },
          { "label": "Medium", "value": "1" }
        ]
      },
			{
        "type": "select",
        "messageKey": "BizTheme",
        "label": "Theme (Bg / Blades)",
        "description": "Selecting your style of Background and Blades",
        "defaultValue": "0",
        "options": [
          { "label": "Black / Black", "value": "0" },
          { "label": "Black / White", "value": "1" },
          { "label": "White / White", "value": "2" },
          { "label": "White / Black", "value": "3" }
        ]
      },
{
        "type": "select",
        "messageKey": "BizChimeInterval",
        "label": "Business Vibration Interval", // 💡 ChimeをVibrationに変更！
        "description": "How often the watch gently vibrates during business hours.", // 💡 「仕事中にどれくらいの頻度で優しく振動するか」に変更！
        "defaultValue": "60",
        "options": [
          { "label": "Every 5 Minutes", "value": "5" },
          { "label": "Every 10 Minutes", "value": "10" },
          { "label": "Every 15 Minutes", "value": "15" },
          { "label": "Every 20 Minutes", "value": "20" }, // 💡 25分を20分に変更！
          { "label": "Every 30 Minutes", "value": "30" },
          { "label": "Every 1 Hour", "value": "60" }
        ]
      },
      {
        "type": "select",
        "messageKey": "BizVibeMode",
        "label": "Business Vibration Mode",
        "defaultValue": "1",
        "options": [
          { "label": "Shake & Periodic Chime", "value": "0" },
          { "label": "Periodic Chime Only", "value": "1" },
          { "label": "Shake Only", "value": "2" },
          { "label": "Disabled", "value": "3" }
        ]
      }
    ]
  },

  
  // ==========================================
  // 4. サポート (Support)
  // ==========================================
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "☕️ Support the Developer"
      },
      {
        "type": "text",
        "defaultValue": "If you enjoy Iris, consider buying me a coffee to keep the gears turning!"
      },
      {
        "type": "text",
        "defaultValue": "<a href='https://ko-fi.com/1987haaa' target='_blank'>☕️ Tap here to buy me a coffee on Ko-fi!</a>"
			}
    ]
  },
  {
    "type": "submit",
    "defaultValue": "SAVE SETTINGS"
  }
];
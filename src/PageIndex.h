// Découpage pour éviter dépassement de longueur de ligne
static const char MAIN_page[] =
"<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<meta name='author' content='Clément Saillant (electron-rare)'>"
"<title>ESP32 P10 Clock</title><style>body{font-family:Arial,Helvetica,sans-serif;margin:0;padding:1rem;background:#111;color:#eee}"
"h1{text-align:center;color:#4caf50;font-size:1.4rem;margin:.2rem 0}fieldset{border:1px solid #333;border-radius:6px;margin:0 0 1rem;padding:.8rem}"
"legend{padding:0 .5rem;color:#4caf50;font-weight:bold}label{display:block;margin-top:.5rem;font-size:.85rem}"
"input,select{width:100%;padding:.4rem;margin-top:.2rem;background:#222;border:1px solid #444;color:#eee;border-radius:4px;font-size:.9rem}"
"button{margin-top:.7rem;width:100%;background:#4caf50;color:#fff;border:none;padding:.55rem .7rem;font-size:.95rem;border-radius:4px;cursor:pointer}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:.5rem}.footer{text-align:center;font-size:.65rem;margin-top:1rem;opacity:.6}" 
"a.link-btn{display:inline-block;margin:.25rem 0;padding:.45rem .6rem;background:#1976d2;color:#fff;text-decoration:none;border-radius:4px;font-size:.75rem}"
"</style><script>function qs(i){return document.getElementById(i);}function send(id){const f=qs(id);const p=new URLSearchParams(new FormData(f));fetch('/settings?'+p.toString()).then(r=>r.text()).then(()=>{qs('msg').textContent='OK';setTimeout(()=>qs('msg').textContent='',1500);});return false;}</script></head><body>"
"<h1>ESP32 P10 RGB Clock</h1><div id='msg' style='text-align:center;font-size:.8rem;height:1rem;'></div>"
"<form id='timeForm' onsubmit=\"return send('timeForm')\"><fieldset><legend>Date & Heure</legend><input type='hidden' name='sta' value='setTimeDate'>"
"<div class='grid'><div><label>Année<input name='d_Year' type='number' min='2024' max='2099' required></label></div><div><label>Mois<input name='d_Month' type='number' min='1' max='12' required></label></div><div><label>Jour<input name='d_Day' type='number' min='1' max='31' required></label></div><div><label>Heure<input name='t_Hour' type='number' min='0' max='23' required></label></div><div><label>Minute<input name='t_Minute' type='number' min='0' max='59' required></label></div><div><label>Seconde<input name='t_Second' type='number' min='0' max='59' required></label></div></div><button>Mettre à l'heure</button></fieldset></form>"
"<form id='brightForm' onsubmit=\"return send('brightForm')\"><fieldset><legend>Luminosité</legend><input type='hidden' name='sta' value='setBrightness'><label>Valeur (0-255)<input name='input_Brightness' type='number' min='0' max='255' value='125'></label><button>Appliquer</button></fieldset></form>"
"<form id='scrollForm' onsubmit=\"return send('scrollForm')\"><fieldset><legend>Texte défilant</legend><input type='hidden' name='sta' value='setScrollingText'><label>Texte (150 max)<input name='input_Scrolling_Text' maxlength='150' value='ESP32 RGB CLOCK'></label><button>Enregistrer</button></fieldset></form>"
"<form id='modeForm' onsubmit=\"return send('modeForm')\"><fieldset><legend>Mode</legend><input type='hidden' name='sta' value='setDisplayMode'><label>Mode<select name='input_Display_Mode'><option value='1'>Statique</option><option value='2'>Cyclique</option></select></label><button>Appliquer</button></fieldset></form>"
"<form id='countForm' onsubmit=\"return send('countForm')\"><fieldset><legend>Countdown</legend><input type='hidden' name='sta' value='setCountdown'><label>Actif<select name='countdown_Active'><option value='false'>Non</option><option value='true'>Oui</option></select></label><div class='grid'><div><label>Année<input name='countdown_Year' type='number' min='2024' max='2099'></label></div><div><label>Mois<input name='countdown_Month' type='number' min='1' max='12'></label></div><div><label>Jour<input name='countdown_Day' type='number' min='1' max='31'></label></div><div><label>Heure<input name='countdown_Hour' type='number' min='0' max='23'></label></div><div><label>Minute<input name='countdown_Minute' type='number' min='0' max='59'></label></div><div><label>Seconde<input name='countdown_Second' type='number' min='0' max='59'></label></div></div><label>Titre<input name='countdown_Title' maxlength='50' value='NEW YEAR'></label><button>Appliquer Countdown</button></fieldset></form>"
"<div class='section-actions'><a class='link-btn' href='/settings?sta=resetSystem'>Redémarrer</a></div><div class='footer'>Firmware Web Minimal &copy; 2025<br>Projet par Clément Saillant (<a href='https://github.com/electron-rare' style='color:#4caf50' target='_blank' rel='noopener'>electron-rare</a>)<br><a href='/about' style='color:#888'>À propos / Licence</a></div></body></html>";

static const char ABOUT_page[] =
"<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'><title>À propos</title><style>body{font-family:Arial;background:#111;color:#eee;padding:1rem;line-height:1.4}a{color:#4caf50}code{background:#222;padding:2px 4px;border-radius:3px}</style></head><body>"
"<h1>À propos</h1>"
"<p><strong>Projet :</strong> ESP32 P10 RGB 32x16 Digital Clock (PlatformIO)</p>"
"<p><strong>Auteur :</strong> Clément Saillant (<a href='https://github.com/electron-rare' target='_blank' rel='noopener'>electron-rare</a>)</p>"
"<h2>Crédits</h2><ul>"
"<li>Panneau piloté via <em>PxMatrix</em></li>"
"<li>RTC via <em>RTClib</em> (Adafruit)</li>"
"<li>Rendu texte via <em>Adafruit GFX</em></li>"
"</ul>"
"<h2>Licence</h2><p>Ce projet est distribué sous licence MIT. Voir le fichier <code>LICENSE</code> dans le dépôt.</p>"
"<pre style='background:#222;padding:.7rem;border-radius:6px;overflow:auto;font-size:.7rem'>MIT License\n\nCopyright (c) 2025 Clément Saillant\n\nPermission is hereby granted, free of charge, to any person obtaining a copy\n... (version abrégée, texte complet dans le dépôt) ...\n</pre>"
"<p><a href='/'>&larr; Retour</a></p>"
"</body></html>";


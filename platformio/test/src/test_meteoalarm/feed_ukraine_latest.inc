/* Real excerpt of the MeteoAlarm Ukraine feed, captured on 2026-08-08.
 * Source: https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-ukraine
 * Timestamps are frozen, expected epochs are hardcoded in the tests.
 * Trimmed from the 131 entries of the live feed to six complete entries:
 * Squall (x2), Hail, Thunderstorm (x2) and Heavy rain warnings. The four
 * entries covering a point in western Ukraine (51.1 N, 24.85 E) are used
 * by the polygon filter tests; the two remaining entries do not cover it.
 * The Squall/Hail/Thunderstorm entries expire exactly at
 * 2026-08-07T18:00:00Z, exercising the expiry boundary at the test clock.
 *
 * Verbatim feed content (entries repeated per covered area), used as a
 * deterministic parser fixture. GPL-3.0 applies to this file as part of
 * the project; the embedded content is licensed CC BY 4.0 by MeteoAlarm.
 */
static const char kFeedUkraineLatest[] PROGMEM = R"FEED(
<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom" xmlns:cap="urn:oasis:names:tc:emergency:cap:1.2">
  <link rel="hub" href="https://pubsubhubbub.appspot.com/"/>
  <link type="application/atom+xml" rel="self" href="https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-ukraine"/>
  <link type="text/html" rel="alternate" href="https://meteoalarm.org"/>
  <rights>Copyright © 2026 MeteoAlarm.Org. Licensed under terms equivalent to CC BY 4.0, with additional requirements for redistributing outlined in our Terms and Conditions.</rights>
  <generator>MeteoAlarm Producer Server</generator>
  <logo>https://feeds.meteoalarm.org/images/logo.svg</logo>
  <author>
    <name>meteoalarm.org</name>
    <uri>https://meteoalarm.org</uri>
    <email>meteoalarm@geosphere.at</email>
  </author>
  <id>tag:meteoalarm.org,2023-11-02:UA</id>
  <title>MeteoAlarm - Alerting Europe for Extreme Weather</title>
  <updated>2026-08-07T21:05:18.379513Z</updated>
  <entry>
    <cap:polygon>51.924393,25.592697 51.89525,25.705876 51.848838,25.640123 51.803519,25.631826 51.755499,25.681337 51.641205,25.608514 51.539964,25.565176 51.479453,25.641801 51.417064,25.646491 51.37769,25.840711 51.318008,25.851092 51.276466,25.934152 51.176884,26.067732 51.123395,25.99473 51.05061,26.091169 50.97318,26.082826 50.935391,26.000699 50.865836,25.991679 50.798409,25.890139 50.687276,25.869181 50.647009,25.823743 50.669689,25.742346 50.724743,25.69396 50.649266,25.352531 50.607412,25.398351 50.583614,25.318768 50.532952,25.270667 50.508276,25.157902 50.460057,25.134991 50.418882,25.166699 50.331113,25.166321 50.287237,25.112613 50.341725,25.016829 50.351232,24.855793 50.33766,24.777279 50.374235,24.721104 50.413614,24.59657 50.493399,24.587769 50.555566,24.516498 50.540906,24.448318 50.591262,24.39957 50.573281,24.317939 50.583578,24.230494 50.633849,24.172295 50.637538,24.097894 50.72549,24.01845 50.834674,23.985205 50.835622,24.093373 50.89208,24.06415 50.951318,23.969638 51.070866,23.912475 51.099615,23.856835 51.158169,23.858504 51.213406,23.743382 51.278227,23.70837 51.327392,23.635595 51.409146,23.6979 51.445433,23.651037 51.577007,23.664658 51.62146,23.604073 51.667488,23.782716 51.642565,23.86926 51.608303,23.880223 51.580074,23.996325 51.666752,24.120621 51.716892,24.270098 51.75109,24.319415 51.813648,24.299224 51.878444,24.372418 51.902651,24.625586 51.881145,24.749604 51.911337,24.918908 51.897961,24.966161 51.953464,25.099755 51.968046,25.264223 51.931761,25.348298 51.924393,25.592697</cap:polygon>
    <link title="Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast" href="https://meteoalarm.org?polygon=131aa7b5-d47f-4240-974b-847e6ff638f8,0,0,13" hreflang="en"/>
    <cap:areaDesc>Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</cap:areaDesc>
    <cap:event>Squall warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T10:44:20+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Update</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.a2jtfk46Dj9934OUUc9m</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/131aa7b5-d47f-4240-974b-847e6ff638f8"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/131aa7b5-d47f-4240-974b-847e6ff638f8?index_info=0&amp;index_area=0&amp;index_polygon=13</id>
    <title>Yellow Wind Warning issued for Ukraine - Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
  <entry>
    <cap:polygon>51.517297,27.66295 51.435069,27.504931 51.34174,27.522214 51.310542,27.440729 51.236304,27.463521 51.14294,27.364017 51.052726,27.317384 51.000176,27.19999 50.895971,27.256251 50.795932,27.233974 50.7223,27.275728 50.670044,27.212762 50.606042,27.224818 50.562096,27.196327 50.551929,27.050872 50.47739,26.73526 50.389428,26.682511 50.368183,26.601749 50.307231,26.548004 50.260478,26.471008 50.264099,26.394927 50.178749,26.273466 50.179699,26.220449 50.227532,26.193 50.255416,26.060483 50.229059,26.019402 50.257225,25.942433 50.191467,25.857238 50.152114,25.62538 50.166158,25.476011 50.053757,25.431952 50.002946,25.349094 50.130848,25.197503 50.20265,25.169584 50.279515,25.204609 50.287237,25.112613 50.331113,25.166321 50.418882,25.166699 50.460057,25.134991 50.508276,25.157902 50.532952,25.270667 50.583614,25.318768 50.607412,25.398351 50.649266,25.352531 50.724743,25.69396 50.669689,25.742346 50.647009,25.823743 50.687276,25.869181 50.798409,25.890139 50.865836,25.991679 50.935391,26.000699 50.97318,26.082826 51.05061,26.091169 51.123395,25.99473 51.176884,26.067732 51.276466,25.934152 51.318008,25.851092 51.37769,25.840711 51.417064,25.646491 51.479453,25.641801 51.539964,25.565176 51.641205,25.608514 51.755499,25.681337 51.803519,25.631826 51.848838,25.640123 51.89525,25.705876 51.924393,25.592697 51.932158,25.989732 51.912237,26.095143 51.862313,26.190807 51.862964,26.371025 51.799808,26.469889 51.824603,26.611442 51.801592,26.752604 51.735933,26.948359 51.768834,26.994577 51.773442,27.201322 51.663852,27.208196 51.60622,27.267278 51.610818,27.455812 51.637176,27.549992 51.592115,27.729501 51.517297,27.66295</cap:polygon>
    <link title="Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast" href="https://meteoalarm.org?polygon=131aa7b5-d47f-4240-974b-847e6ff638f8,0,0,12" hreflang="en"/>
    <cap:areaDesc>Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</cap:areaDesc>
    <cap:event>Squall warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T10:44:20+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Update</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.a2jtfk46Dj9934OUUc9m</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/131aa7b5-d47f-4240-974b-847e6ff638f8"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/131aa7b5-d47f-4240-974b-847e6ff638f8?index_info=0&amp;index_area=0&amp;index_polygon=12</id>
    <title>Yellow Wind Warning issued for Ukraine - Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
  <entry>
    <cap:polygon>51.924393,25.592697 51.89525,25.705876 51.848838,25.640123 51.803519,25.631826 51.755499,25.681337 51.641205,25.608514 51.539964,25.565176 51.479453,25.641801 51.417064,25.646491 51.37769,25.840711 51.318008,25.851092 51.276466,25.934152 51.176884,26.067732 51.123395,25.99473 51.05061,26.091169 50.97318,26.082826 50.935391,26.000699 50.865836,25.991679 50.798409,25.890139 50.687276,25.869181 50.647009,25.823743 50.669689,25.742346 50.724743,25.69396 50.649266,25.352531 50.607412,25.398351 50.583614,25.318768 50.532952,25.270667 50.508276,25.157902 50.460057,25.134991 50.418882,25.166699 50.331113,25.166321 50.287237,25.112613 50.341725,25.016829 50.351232,24.855793 50.33766,24.777279 50.374235,24.721104 50.413614,24.59657 50.493399,24.587769 50.555566,24.516498 50.540906,24.448318 50.591262,24.39957 50.573281,24.317939 50.583578,24.230494 50.633849,24.172295 50.637538,24.097894 50.72549,24.01845 50.834674,23.985205 50.835622,24.093373 50.89208,24.06415 50.951318,23.969638 51.070866,23.912475 51.099615,23.856835 51.158169,23.858504 51.213406,23.743382 51.278227,23.70837 51.327392,23.635595 51.409146,23.6979 51.445433,23.651037 51.577007,23.664658 51.62146,23.604073 51.667488,23.782716 51.642565,23.86926 51.608303,23.880223 51.580074,23.996325 51.666752,24.120621 51.716892,24.270098 51.75109,24.319415 51.813648,24.299224 51.878444,24.372418 51.902651,24.625586 51.881145,24.749604 51.911337,24.918908 51.897961,24.966161 51.953464,25.099755 51.968046,25.264223 51.931761,25.348298 51.924393,25.592697</cap:polygon>
    <link title="Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast" href="https://meteoalarm.org?polygon=84cac034-4dc6-4720-9cc1-9d9ff4ea46a9,0,0,13" hreflang="en"/>
    <cap:areaDesc>Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</cap:areaDesc>
    <cap:event>Hail warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T10:44:20+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Update</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.6VZUdYN1V6PA9tBQgQ6u</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/84cac034-4dc6-4720-9cc1-9d9ff4ea46a9"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/84cac034-4dc6-4720-9cc1-9d9ff4ea46a9?index_info=0&amp;index_area=0&amp;index_polygon=13</id>
    <title>Yellow Thunderstorm Warning issued for Ukraine - Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
  <entry>
    <cap:polygon>51.924393,25.592697 51.89525,25.705876 51.848838,25.640123 51.803519,25.631826 51.755499,25.681337 51.641205,25.608514 51.539964,25.565176 51.479453,25.641801 51.417064,25.646491 51.37769,25.840711 51.318008,25.851092 51.276466,25.934152 51.176884,26.067732 51.123395,25.99473 51.05061,26.091169 50.97318,26.082826 50.935391,26.000699 50.865836,25.991679 50.798409,25.890139 50.687276,25.869181 50.647009,25.823743 50.669689,25.742346 50.724743,25.69396 50.649266,25.352531 50.607412,25.398351 50.583614,25.318768 50.532952,25.270667 50.508276,25.157902 50.460057,25.134991 50.418882,25.166699 50.331113,25.166321 50.287237,25.112613 50.341725,25.016829 50.351232,24.855793 50.33766,24.777279 50.374235,24.721104 50.413614,24.59657 50.493399,24.587769 50.555566,24.516498 50.540906,24.448318 50.591262,24.39957 50.573281,24.317939 50.583578,24.230494 50.633849,24.172295 50.637538,24.097894 50.72549,24.01845 50.834674,23.985205 50.835622,24.093373 50.89208,24.06415 50.951318,23.969638 51.070866,23.912475 51.099615,23.856835 51.158169,23.858504 51.213406,23.743382 51.278227,23.70837 51.327392,23.635595 51.409146,23.6979 51.445433,23.651037 51.577007,23.664658 51.62146,23.604073 51.667488,23.782716 51.642565,23.86926 51.608303,23.880223 51.580074,23.996325 51.666752,24.120621 51.716892,24.270098 51.75109,24.319415 51.813648,24.299224 51.878444,24.372418 51.902651,24.625586 51.881145,24.749604 51.911337,24.918908 51.897961,24.966161 51.953464,25.099755 51.968046,25.264223 51.931761,25.348298 51.924393,25.592697</cap:polygon>
    <link title="Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast" href="https://meteoalarm.org?polygon=fc77baae-286e-47fd-8949-95e83f45ba33,0,0,13" hreflang="en"/>
    <cap:areaDesc>Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</cap:areaDesc>
    <cap:event>Thunderstorm warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-07T18:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T10:44:20+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Update</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.GuMW2UGRT07CMNGUgP54</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/fc77baae-286e-47fd-8949-95e83f45ba33"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/fc77baae-286e-47fd-8949-95e83f45ba33?index_info=0&amp;index_area=0&amp;index_polygon=13</id>
    <title>Yellow Thunderstorm Warning issued for Ukraine - Cherkasy oblast, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Sumy oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
  <entry>
    <cap:polygon>48.902014,33.89376 48.802825,33.85075 48.792578,33.656517 48.766411,33.581329 48.680737,33.750361 48.627052,33.763323 48.568115,33.509625 48.533614,33.469671 48.437802,33.488786 48.357559,33.459699 48.234727,33.546467 48.170542,33.431551 48.153689,33.271759 48.092046,33.224868 48.083322,33.153248 48.033881,33.036315 47.989118,33.004678 48.048235,32.990162 48.027615,32.883355 47.985527,32.894409 47.981839,32.689495 47.921522,32.651905 47.872984,32.685536 47.819936,32.657796 47.789565,32.427727 47.823596,32.302331 47.81465,32.236322 47.760404,32.231583 47.75591,32.116976 47.817254,32.054674 47.782798,31.837703 47.867385,31.891827 47.963581,31.84383 47.944315,31.751933 48.035465,31.729029 48.102124,31.767284 48.128763,31.582615 48.113349,31.325915 48.117032,31.236334 48.170749,31.220584 48.229278,31.137992 48.22214,30.981017 48.16624,30.930292 48.167779,30.792506 48.191394,30.785389 48.185253,30.647467 48.156441,30.616652 48.179548,30.526799 48.141602,30.307809 48.154645,30.188586 48.150306,30.049495 48.210717,30.007714 48.233298,29.930305 48.186529,29.892185 48.208537,29.780591 48.299337,29.7505 48.369925,29.786996 48.450097,29.951312 48.471673,29.96791 48.482826,30.04984 48.451988,30.1114 48.50378,30.162857 48.527054,30.386594 48.573834,30.402837 48.578975,30.563157 48.627792,30.529402 48.6632,30.598646 48.751871,30.637032 48.769886,30.753329 48.75007,30.831169 48.764992,30.995714 48.728916,31.117039 48.761393,31.171582 48.758667,31.310351 48.727671,31.382153 48.788513,31.467988 48.804215,31.528224 48.906129,31.594792 48.896306,31.689451 48.944618,31.807709 48.906185,31.918235 48.921175,32.079683 48.966126,32.134746 48.989572,32.218961 49.062044,32.237958 49.079058,32.3483 49.039916,32.369909 48.950748,32.582768 48.980459,32.845228 49.067199,32.829193 49.121158,32.889152 49.144397,32.774982 49.191347,32.78303 49.246188,32.856102 49.216821,32.982615 49.18416,33.031174 49.181638,33.107705 49.095047,33.166466 49.083317,33.277762 49.026988,33.334744 48.961316,33.325156 48.958372,33.420263 48.926583,33.497256 48.916889,33.589208 48.973991,33.637943 48.940046,33.712369 48.936156,33.821911 48.902014,33.89376</cap:polygon>
    <link title="Cherkasy oblast, Kharkiv oblast, Kirovohrad oblast, Poltava oblast, Sumy oblast" href="https://meteoalarm.org?polygon=1386487f-aa16-440f-8f2e-fff9f56b3a27,0,0,4" hreflang="en"/>
    <cap:areaDesc>Cherkasy oblast, Kharkiv oblast, Kirovohrad oblast, Poltava oblast, Sumy oblast</cap:areaDesc>
    <cap:event>Heavy rain warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-08T18:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-08T06:00:00+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Alert</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.jh6PXYgSl5UAJ6i4Gc2w</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/1386487f-aa16-440f-8f2e-fff9f56b3a27"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/1386487f-aa16-440f-8f2e-fff9f56b3a27?index_info=0&amp;index_area=0&amp;index_polygon=4</id>
    <title>Yellow Rain Warning issued for Ukraine - Cherkasy oblast, Kharkiv oblast, Kirovohrad oblast, Poltava oblast, Sumy oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
  <entry>
    <cap:polygon>51.924393,25.592697 51.89525,25.705876 51.848838,25.640123 51.803519,25.631826 51.755499,25.681337 51.641205,25.608514 51.539964,25.565176 51.479453,25.641801 51.417064,25.646491 51.37769,25.840711 51.318008,25.851092 51.276466,25.934152 51.176884,26.067732 51.123395,25.99473 51.05061,26.091169 50.97318,26.082826 50.935391,26.000699 50.865836,25.991679 50.798409,25.890139 50.687276,25.869181 50.647009,25.823743 50.669689,25.742346 50.724743,25.69396 50.649266,25.352531 50.607412,25.398351 50.583614,25.318768 50.532952,25.270667 50.508276,25.157902 50.460057,25.134991 50.418882,25.166699 50.331113,25.166321 50.287237,25.112613 50.341725,25.016829 50.351232,24.855793 50.33766,24.777279 50.374235,24.721104 50.413614,24.59657 50.493399,24.587769 50.555566,24.516498 50.540906,24.448318 50.591262,24.39957 50.573281,24.317939 50.583578,24.230494 50.633849,24.172295 50.637538,24.097894 50.72549,24.01845 50.834674,23.985205 50.835622,24.093373 50.89208,24.06415 50.951318,23.969638 51.070866,23.912475 51.099615,23.856835 51.158169,23.858504 51.213406,23.743382 51.278227,23.70837 51.327392,23.635595 51.409146,23.6979 51.445433,23.651037 51.577007,23.664658 51.62146,23.604073 51.667488,23.782716 51.642565,23.86926 51.608303,23.880223 51.580074,23.996325 51.666752,24.120621 51.716892,24.270098 51.75109,24.319415 51.813648,24.299224 51.878444,24.372418 51.902651,24.625586 51.881145,24.749604 51.911337,24.918908 51.897961,24.966161 51.953464,25.099755 51.968046,25.264223 51.931761,25.348298 51.924393,25.592697</cap:polygon>
    <link title="Carpathians mountain, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv city, Kyiv oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast" href="https://meteoalarm.org?polygon=526cddc8-5af9-4f66-8baa-918dae7768cf,0,0,14" hreflang="en"/>
    <cap:areaDesc>Carpathians mountain, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv city, Kyiv oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</cap:areaDesc>
    <cap:event>Thunderstorm warning</cap:event>
    <cap:sent>2026-08-07T10:44:21+00:00</cap:sent>
    <cap:expires>2026-08-08T06:00:00+00:00</cap:expires>
    <cap:effective>2026-08-07T10:44:21+00:00</cap:effective>
    <cap:onset>2026-08-07T18:00:00+00:00</cap:onset>
    <cap:certainty>Likely</cap:certainty>
    <cap:severity>Moderate</cap:severity>
    <cap:urgency>Future</cap:urgency>
    <cap:scope>Public</cap:scope>
    <cap:message_type>Alert</cap:message_type>
    <cap:status>Actual</cap:status>
    <cap:identifier>2.49.0.0.804.0.GB.260807104421.lMolO9vrtkq0Ob8UEwC0</cap:identifier>
    <link type="application/cap+xml" href="https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/526cddc8-5af9-4f66-8baa-918dae7768cf"/>
    <link title="Ukraine" rel="related" href="https://meteoalarm.org?region=UA" hreflang="en"/>
    <author>
      <name>meteoalarm.org</name>
      <uri>https://meteoalarm.org</uri>
    </author>
    <published>2026-08-07T10:44:21Z</published>
    <id>https://feeds.meteoalarm.org/api/v1/warnings/feeds-ukraine/526cddc8-5af9-4f66-8baa-918dae7768cf?index_info=0&amp;index_area=0&amp;index_polygon=14</id>
    <title>Yellow Thunderstorm Warning issued for Ukraine - Carpathians mountain, Chernihiv oblast, Chernivtsi oblast, Ivano-Frankivsk oblast, Khmelnytskyi oblast, Kyiv city, Kyiv oblast, Kyiv oblast and city, Lviv oblast, Rivne oblast, Ternopil oblast, Vinnytsia oblast, Volyn oblast, Zakarpattia oblast, Zhytomyr oblast</title>
    <updated>2026-08-07T10:44:21Z</updated>
  </entry>
</feed>
)FEED";

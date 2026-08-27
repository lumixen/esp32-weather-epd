# MeteoSwiss fixtures

`plz_detail_800100.json` was captured from
`https://app-prod-ws.meteoswiss-app.ch/v1/plzDetail?plz=800100` on 2026-08-27.
`VQHA80_KLO.csv` contains the official SwissMetNet response from
`https://data.geo.admin.ch/ch.meteoschweiz.messwerte-aktuell/VQHA80.csv`,
captured on 2026-08-27 and retained with the `KLO` row used by the tests.

The response is from the MeteoSwiss App backend and the observation file is
provided by the Swiss Federal Office of Meteorology and Climatology
(MeteoSwiss). These fixtures are used only for deterministic parser tests.

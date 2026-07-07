# CatRotator upstream attribution

MadModem v4.13n begins integrating CatRotator ideas as a native MM rotator module.

Upstream package supplied by the user: CatRotator-main.zip.
CatRotator is GPLv3+ Qt/Hamlib software.

MadModem does not embed CatRotator's bundled Hamlib DLLs. The integrated module uses MadModem's existing Hamlib build and packaging path.

The standalone CatRotator WSJT-X/AirScout/PreviSat UDP tracking model is intentionally not carried into the integrated MM UI. In MadModem, rotator tracking is driven by MM's selected QSO target, manual operator commands and future MM Flow Studio guarded actions.

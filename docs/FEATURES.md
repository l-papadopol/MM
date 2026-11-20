# Receiver, station and logbook tools

These tools include contributions by Costantino Cerrotta, IC8TEM. Menu labels
below use the English interface; translations are available in the application.

| Tool | Location | Operation |
| --- | --- | --- |
| RX noise blanker | Decode → Noise Blanker | Select NB2-style impulse blanking or DL2FW linear prediction and a threshold. Disabled by default; compare reception before choosing a level. |
| MSK144 carrier notch | Decode → MSK144 Carrier Notch (ZAP) | Suppress persistent narrow carriers, with detection safeguards for MSK signals. |
| RF power and meters | Tools → Power and SWR | CAT power presets, per-band memory, Po/SWR/ALC readouts, reduced Tune power and SWR protection. Availability depends on rig and Hamlib support. |
| Locator sources | Tools → Locator sources | Use received locators, CALL3.TXT, JTDX locator data and optional G7VRD lookup, with source indication. |
| LoTW users | Tools → LoTW users | Download the ARRL users list; show LoTW markers and optionally filter FT results. |
| FT call-again | Tools | Restart a stopped FT exchange when the selected DX is decoded again, up to three times. This is separate from Evil's CQ queue. |
| Worked-before reset | Tools | Set a UTC cutoff for worked-before, country and square indicators. Existing ADIF QSOs remain in the logbook. |
| MSK144 ping duration | MSK144 decode list | The ms column shows duration in 20 ms steps. |
| YA-NEC theme | Settings → Logbook / FT colours → Theme | Select the light theme contributed by IC8TEM. |
| Split | Settings → Audio / PTT / CAT → Split operation | Rig/Fake It for FT8, FT4, MSK144 and Q65. Fake It uses CAT to keep TX audio between 1500 and 2000 Hz with 500 Hz dial steps. |

Fake It is the default with CAT enabled. The existing one-time settings migration
can change a previously stored None to Fake It; a subsequent explicit choice of
None is retained. Without CAT, split is inactive. Check the selected split and
your radio's behavior before transmitting.

The noise blanker adds processing delay, which is compensated in decoder timing.
Filtering can change received signals; the safeguards do not guarantee identical
decodes in every noise condition.

FT8/FT4 sequencing, priority queues and Evil Auto QSO remain available with
Doppler correction. For automation traces and compressed diagnostics, see
[Troubleshooting](TROUBLESHOOTING.md) and the built-in FT8/FT4 Help.

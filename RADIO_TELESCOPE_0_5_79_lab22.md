# MadModem 0.5.79-lab22 - Radio Telescope honeycomb + peak pointing

This lab refines the Radio Telescope prototype:

- The heatmap renderer now draws a screen-space honeycomb grid clipped to the sky disk or azimuth ring, then maps each hex cell to the nearest measured az/el tile. This keeps adjacent hex sides visually continuous while still using the antenna beam width for the scan step.
- Rotator profiles now contain an antenna beam width field. Radio Telescope uses the active rotator profile beam width by default and writes changes back to the active profile.
- The Mode tab now offers `Point max` and `Auto-point strongest tile after scan`.
- Peak pointing moves the connected rotator to the strongest measured tile using the existing rotator controller.

The scan itself remains conservative step-and-stare: move, wait estimated rotator time + configured settle, measure band noise, fill tile. A future lab can add a fine local hill-climb around the strongest tile.

[![Build status](https://ci.appveyor.com/api/projects/status/fcya1k80q0lhvx3y?svg=true)](https://ci.appveyor.com/project/MaverickTse/aviutlmanualtracking)

# AviUtlManualTracking
a cursor location logger to assist object tracking in AviUtl

## What it does
When a video is loaded and the plugin is activated, it records the cursor position relative to the video. Top left of the video is seen as (0,0) with positive width and positive height. Besides coordinate, it also logs SIZE(width and height) and ROTATION(as degree). The __Region-of-Interest(ROI)__ is visualized using a bounding box with customizeable color. The data is continuously written into a CSV file to avoid excessive memory consumption.

## Building
1. Install CMake, VS2015/VS2017
2. Use CMake-gui to generate a VisualStudio solution
3. Open up the solution, choose "Release" configuration, then press F7 to build
4. find the generated manualtracking.auf, copy it to AviUtl's folder
5. Run AviUtl and test

## Dependencies
- VC2015 or VC2017 runtimes
- DirectXMath Library (for drawing the box)

## How to Use
1. Load a video, estimate the size of ROI
2. Set width and height to roughly match the ROI
3. Activate the plugin
4. Left-click on the center of ROI, a green box should appear
5. Press :arrow_right: to play through frames, __move your mouse__ to follow the ROI __at the same time__
6. [optional] if the ROI twisted significantly, press :arrow_up: or :arrow_down: to rotate the bounding box. __If you use this feature, please disable the AviUtl shortcut-key association for these two buttons.__
7. Continue until you are done or missed or the ROI change size or rotation or location abruptly
8. Left-click again to stop recording
9. You may either:
 * Move back a few frames and Goto 4 again
 * Save the data by clicking the Save CSV button
 * Discard the logged data by clicking Clear Memory __(the logged data presist until you click Clear Memory or manually delete a CSV file after you close AviUtl)__
10. Use program of your choice to analyze and transform the data to your heart's content
11. Give me a job

## Conversion to EXO
Whenever I write a tracking plugin, I find myself reimplementing the same EXO conversion part again and again... so this time I won't build it into the plugin. Later either me or somebody else may fill that gap. Python or R would be a good tool for the job as they are rich in maths and stat libraries.

## CSV Format
The CSV file does not have title. The fields are:

```Frame no., x, y, width, height, rotation(degree)```

Frame no. are not unique, while rotation should be within 0 and 359

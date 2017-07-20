[![Build status](https://ci.appveyor.com/api/projects/status/fcya1k80q0lhvx3y?svg=true)](https://ci.appveyor.com/project/MaverickTse/aviutlmanualtracking)    [![Codacy Badge](https://api.codacy.com/project/badge/Grade/f4a642c7be7849fca7418bb69c9d32d4)](https://www.codacy.com/app/MaverickTse/AviUtlManualTracking?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=MaverickTse/AviUtlManualTracking&amp;utm_campaign=Badge_Grade)

# AviUtlManualTracking
a cursor location logger to assist object tracking in AviUtl

## What it does
When a video is loaded and the plugin is activated, it records the cursor position relative to the video. Top left of the video is seen as (0,0) with positive width and positive height. Besides coordinate, it also logs SIZE(width and height) and ROTATION(as degree). The __Region-of-Interest(ROI)__ is visualized using a bounding box with customizeable color. The data is continuously written into a CSV file to avoid excessive memory consumption.

## Building
0. __Just get it from the Release page! Do the following if you are a developer__
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
7. Continue until you are done/missed/ROI change abruptly/ROI disappear
8. Left-click again to stop recording
9. You may either:
 * Save the data by clicking the Save CSV button (recommended)
 * Move back a few frames and Goto 4 again
 * Discard the logged data by clicking Clear Memory __(the logged data presist until you click Clear Memory or manually delete a CSV file after you close AviUtl)__
10. Use program of your choice to analyze and transform the data to your heart's content
11. Give me a job

## Conversion to EXO
The inlcuded python script, csv2exo.py can be used to generate EXO from the saved CSV.

__REQUIREMENT:__ Anaconda 4.4 or above (Vanilla Python3 will not run as it use Numpy, PANDAS, etc extensively)

To save EXO for __Japanese__ exedit use, at minimal:
```
python3 csv2exo.py C:\path\to\your.csv --width=1280 --height=720 --fps=30
```
For __English-modded__ version, use:
```
python3 csv2exo.py C:\path\to\your.csv --width=1280 --height=720 --fps=30 --eng
```
The script would perform extensive calculation on the tracked data in order to cut down on data points. To see if the simplified points resemble the original data prior saving __(HIGHLY RECOMMENDED)__, use:
```
python3 csv2exo.py C:\path\to\your.csv --preview
```
This would generate a 3D-plot with two lines. The more they overlap, the better, but usually retaining more data points. The major parameter for the good-of-fitness and point count trade-off is ```--simplify```. By default I set it to 1.0. You can try a range of values __from 0.0 to 5.0__, but over-simplification would often occur beyond 4.0.

The second factor is ```--smooth```, which is the window size of running-average. Does not affect much usually, unless the original line is really noisy. Deafult to 3.


Each CSV would generate ONE and ONLY ONE object on layer 1, with multiple mid-points in-between.

## More EXO Tips
- the conversion script supports a target frame size and fps different from the original one: ```--new_width --new_height --new_fps```
- The AviUtl installer bundled an EXOStacker program in the /AviUtlInstalledFolder/Utility folder. Use it to combine all your saved EXOs from the same project. Suggest also include an EXO that holds your video, then all video and tracking data can be merged in one-go
- The script implemented Visvalingam-Whyatt polyline simplification using 3D-points (x, y, frame number)


## CSV Format
The CSV file does not have title. The fields are:

```Frame no., x, y, width, height, rotation(degree)```

Frame no. are not unique, while rotation should be within ±0 ~ 359

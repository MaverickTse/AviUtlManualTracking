# -*- coding: utf-8 -*-
import os
import sys
import gc
import typing
import itertools
import argparse
import codecs
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from operator import itemgetter

EXO_HEADER = u"""\
[exedit]
width={w}
height={h}
rate={fps}
scale=1
length={last_frame}
audio_rate={arate}
audio_ch={ach}

"""


EXO_TEMPLATE = {
    'en': """\
[{obj_id}]
start={fs}
end={fe}
layer=1
overlay=1
camera=0
{chain}
[{obj_id}.0]
_name=Graphic
Size=100
rAspect=0.0
Line width=4000
type=2
color=ffffff
name=
[{obj_id}.1]
_name=Resize
Zoom%=100.00
X={ws},{we},1
Y={hs},{he},1
No interpolation=0
Specified size by the number of dots=1
[{obj_id}.2]
_name=Standard drawing
X={xs},{xe},1
Y={ys},{ye},1
Z=0.0
Zoom%=100.00
Clearness=70.0
Rotation={rs},{re},1
blend=0
""",
    'jp': u"""\
[{obj_id}]
start={fs}
end={fe}
layer=1
overlay=1
camera=0
{chain}
[{obj_id}.0]
_name=図形
サイズ=100
縦横比=0.0
ライン幅=4000
type=2
color=ffffff
name=
[{obj_id}.1]
_name=リサイズ
拡大率=100.00
X={ws},{we},1
Y={hs},{he},1
補間なし=0
ドット数でサイズ指定=1
[{obj_id}.2]
_name=標準描画
X={xs},{xe},1
Y={ys},{ye},1
Z=0.0
拡大率=100.00
透明度=70.0
回転={rs},{re},1
blend=0
"""
}


def clean_data(options_dict: dict) -> pd.DataFrame:
    if "csvfile" not in options_dict:
        raise ValueError('Missing essential option "csvfile"')
    if not os.path.isfile(options_dict["csvfile"]):
        raise ValueError('Input is not a valid file', options_dict["csvfile"])
    if not os.path.exists(options_dict["csvfile"]):
        raise ValueError('Input file not found', options_dict["csvfile"])

    raw = pd.read_csv(options_dict["csvfile"], header=None, names=["frame", "x", "y", "w", "h", "r"], dtype=np.float32)
    cleaned = raw.groupby(["frame"], sort=False, as_index=False).last()
    del raw
    gc.collect()
    if ("smooth" in options_dict) and (options_dict["smooth"] > 1):
        cleaned.rolling(window=options_dict["smooth"], min_periods=1, center=True, on="x").mean()
        cleaned.rolling(window=options_dict["smooth"], min_periods=1, center=True, on="y").mean()

    return cleaned


def write_csv(options_dict: dict, df: pd.DataFrame):
    out_file_name = None
    if ("o" not in options_dict) or (not options_dict["o"]):
        out_file_name = os.path.join(os.path.dirname(options_dict["csvfile"]),
                                     os.path.splitext(os.path.basename(options_dict["csvfile"]))[0]+'_cleaned.csv')
    else:
        out_file_name = options_dict["o"]

    df.to_csv(out_file_name)
    gc.collect()


def calc_triangle_area(v0: np.array, v1: np.array, v2: np.array) -> np.float32:
    """
    Calculate triangle area from 3 points/vector in 3D space
    :param v0: numpy array [x0, y0, z0] for vertex A
    :param v1: numpy array [x1, y1, z1] for vertex B
    :param v2: numpy array [x2, y2, z2] for vertex C
    :return: Area of triangle as np.float32
    """
    return np.float32(np.linalg.norm(np.cross((v1-v0), (v2-v0))) / 2.0)


def calc_area3d(sx: pd.Series, sy: pd.Series, sz: pd.Series) -> pd.Series:
    """
    DataFrame version for calc_triangle area.
    Example: df["foo"] = calc_area3d(data.x, data.y, data.z)
    :param sx: a series of x-coordinate
    :param sy: a series of y-coordinate
    :param sz: a series of y-coordinate
    :return: a PANDAS Series of triangle area
    """
    area = []
    for x0, y0, z0, x1, y1, z1, x2, y2, z2 in zip(sx.shift(-1), sy.shift(-1), sz.shift(-1),
                                                  sx, sy, sz,
                                                  sx.shift(1), sy.shift(1), sz.shift(1)):
        area.append(
            calc_triangle_area(np.array([x0, y0, z0]), np.array([x1, y1, z1]), np.array([x2, y2, z2]))
        )
    return pd.Series(area)


def polyline_simplification(options_dict: dict, df: pd.DataFrame) -> pd.DataFrame:
    threshold = options_dict['simplify']
    # Rank, -0, -Th, Rank', max(a,b)-> Rank, -0, -Th
    satisfy_threshold = False
    df2 = df.copy()
    if "rank" not in df2:
        df2['rank'] = calc_area3d(df2.x, df2.y, df2.frame)
        df2.query("rank != 0", inplace=True)
        df2.query("(rank >= @threshold) or (rank != rank)", inplace=True)
    else:
        while not satisfy_threshold:
            df2['A2'] = calc_area3d(df2.x, df2.y, df2.frame)  # re-calc after removal
            df2['rank'] = df2[['rank', 'A2']].apply(max, axis=1)  # keep the larger value
            rows_before_removal = df2.shape[0]
            df2.query("rank != 0", inplace=True)  # remove 0
            df2.query("(rank >= @threshold) or (rank != rank)", inplace=True)  # remove under threshold keep NaN
            rows_after_removal = df2.shape[0]
            if rows_after_removal == rows_before_removal:  # i.e. nothing removed
                satisfy_threshold = True
    return df2


def visualize(options_dict: dict, df1: pd.DataFrame, df2: pd.DataFrame):
    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1, projection='3d')
    ax.plot(df1['x'], df1['y'], df1['frame'], linewidth=3, color='red', label='smoothed')
    ax.plot(df2['x'], df2['y'], df2['frame'], linewidth=2, color='lime', label='simplified')
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Frame')
    title = "Smooth= {winsize}\nSimplify={sim_th}\nSrc pt: {src} Simplified pt: {spt}".format(
        winsize=options_dict["smooth"], sim_th=options_dict["simplify"], src=df1.shape[0], spt=df2.shape[0]
    )
    ax.set_title(title, loc='left')
    ax.legend()
    plt.show()


def data_transform(options_dict: dict, simplified_data: pd.DataFrame) -> pd.DataFrame:
    df = simplified_data.copy()
    dft = pd.DataFrame()
    segments = df.shape[0] - 1
    dft['frame_s'] = df.frame[:-1]
    dft['frame_e'] = df.frame.shift(-1)[:-1] - 1
    dft['resize_x_s'] = df.w[:-1]
    dft['resize_x_e'] = df.w.shift(-1)[:-1]
    dft['resize_y_s'] = df.h[:-1]
    dft['resize_y_e'] = df.h.shift(-1)[:-1]
    dft['x_s'] = df.x[:-1]
    dft['x_e'] = df.x.shift(-1)[:-1]
    dft['y_s'] = df.y[:-1]
    dft['y_e'] = df.y.shift(-1)[:-1]
    dft['r_s'] = df.r[:-1]
    dft['r_e'] = df.r.shift(-1)[:-1]
    dft['id'] = pd.Series(range(0, segments), index=dft.index)
    return dft


def aviutl_correction(options_dict: dict, transformed_data: pd.DataFrame) -> pd.DataFrame:
    if "width" not in options_dict:
        print("Width parameter is missing for EXO export")
        sys.exit(1)
    if "height" not in options_dict:
        print("Height parameter is missing for EXO export")
        sys.exit(1)
    if "fps" not in options_dict:
        print("fps parameter is missing for EXO export")
        sys.exit(1)

    x_shift = options_dict["width"] / -2
    y_shift = options_dict["height"] / -2
    x_scale = 1.0
    y_scale = 1.0
    fps_scale = 1.0
    if "new_width" in options_dict:
        x_scale = options_dict["new_width"] / options_dict["width"]
    if "new_height" in options_dict:
        x_scale = options_dict["new_height"] / options_dict["height"]
    if "new_fps" in options_dict:
        fps_scale = options_dict["new_fps"] / options_dict["fps"]

    df = transformed_data.copy()
    df['x_s'] = (df['x_s'] + x_shift) * x_scale
    df['x_e'] = (df['x_e'] + x_shift) * x_scale
    df['resize_x_s'] = df['resize_x_s'] * x_scale
    df['resize_x_e'] = df['resize_x_e'] * x_scale

    df['y_s'] = (df['y_s'] + y_shift) * y_scale
    df['y_e'] = (df['y_e'] + y_shift) * y_scale
    df['resize_y_s'] = df['resize_y_s'] * y_scale
    df['resize_y_e'] = df['resize_y_e'] * y_scale

    df['frame_s'] = (df['frame_s'] * fps_scale) + 1
    df['frame_e'] = (df['frame_e'] * fps_scale) + 1
    return df


def write_exo(options_dict: dict, object_table: pd.DataFrame):
    out_file_name = None
    if ("o" not in options_dict) or (not options_dict["o"]):
        out_file_name = os.path.join(os.path.dirname(options_dict["csvfile"]),
                                     os.path.splitext(os.path.basename(options_dict["csvfile"]))[0] + '.exo')
    else:
        out_file_name = options_dict["o"]

    content = u""
    language = "jp"
    if "eng" in options_dict:
        language = "en"

    e_w = options_dict["new_width"] if "new_width" in options_dict else options_dict["width"]
    e_h = options_dict["new_height"] if "new_height" in options_dict else options_dict["height"]
    e_fps = options_dict["new_fps"] if "new_fps" in options_dict else options_dict["fps"]
    e_lastframe = int(object_table.frame_e.tail(1))

    content += EXO_HEADER.format(w=e_w, h=e_h, fps=e_fps, last_frame=e_lastframe,
                                 arate=options_dict["audio_rate"], ach=options_dict["audio_ch"])
    for row in object_table.itertuples():
        e_chain = "chain=1" if row.id != 0 else ""
        content += EXO_TEMPLATE[language].format(
            obj_id=int(row.id),
            fs=int(row.frame_s),
            fe=int(row.frame_e),
            chain=e_chain,
            ws=round(row.resize_x_s, 2),
            we=round(row.resize_x_e, 2),
            hs=round(row.resize_y_s, 2),
            he=round(row.resize_y_e, 2),
            xs=round(row.x_s, 2),
            xe=round(row.x_e, 2),
            ys=round(row.y_s, 2),
            ye=round(row.y_e, 2),
            rs=round(row.r_s, 2),
            re=round(row.r_e, 2),
        )

    with codecs.open(out_file_name, 'w', 'shift_jis') as f:
        f.write(content)
    print("EXO exported to ", out_file_name)


if __name__ == "__main__":
    cli_parser = argparse.ArgumentParser(description="Clean CSV coordinate data and transform into AviUtl EXO")
    cli_parser.add_argument("csvfile", help="the input CSV file to be processed")
    cli_parser.add_argument("--width", type=int, default=argparse.SUPPRESS, help="Width of original video")
    cli_parser.add_argument("--height", type=int, default=argparse.SUPPRESS, help="Height of original video")
    cli_parser.add_argument("--fps", type=int, default=argparse.SUPPRESS, help="Frame rate of original video")
    cli_parser.add_argument("--new_fps", type=int, default=argparse.SUPPRESS, help="New frame rate of original video")
    cli_parser.add_argument("--new_width", type=int, default=argparse.SUPPRESS, help="New width for exported EXO")
    cli_parser.add_argument("--new_height", type=int, default=argparse.SUPPRESS, help="New height for exported EXO")
    cli_parser.add_argument("--audio_rate", type=int, default=44100, help="Audio sample rate for exported EXO")
    cli_parser.add_argument("--audio_ch", type=int, default=2, help="Audio channel count for exported EXO")
    cli_parser.add_argument("-o", default=argparse.SUPPRESS, help="Output filename")
    cli_parser.add_argument("--clean-only", action="store_true", default=argparse.SUPPRESS,
                            help="Remove duplicated frame data in the CSV without conversion to EXO")
    cli_parser.add_argument("--smooth", type=int, choices=range(2, 120), default=3,
                            help="The sliding-window size for roll-averaging positional data [0~120]")
    cli_parser.add_argument("--simplify", type=float, default=1,
                            help="A small threshold value (float number) for polyline simplification")

    cli_parser.add_argument("--eng", action="store_true", default=argparse.SUPPRESS,
                            help="When exporting EXO, target for English-modded Advanced Editing")
    cli_parser.add_argument("--preview", action="store_true", default=argparse.SUPPRESS,
                            help="View cleaned and simplified data plot then exit")

    program_options = vars(cli_parser.parse_args())

    cleaned = None
    if "clean-only" in program_options:
        write_csv(program_options, clean_data(program_options))
        print("Clean CSV written out")
        sys.exit(0)
    else:
        cleaned = clean_data(program_options)
        simplified = polyline_simplification(program_options, cleaned)
        print('points in smoothed data: ', cleaned.shape[0])
        print('points in simplified data:', simplified.shape[0])
        if "preview" in program_options:
            visualize(program_options, cleaned, simplified)
            sys.exit(0)
        obj_table = data_transform(program_options, simplified)
        obj_table_adj = aviutl_correction(program_options, obj_table)
        write_exo(program_options, obj_table_adj)


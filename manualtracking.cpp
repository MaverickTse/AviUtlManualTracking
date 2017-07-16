#include "filter.h"
#include <windowsx.h>
#include <DirectXMath.h>
#include <fstream>
#include <vector>
#include <utility>
#include <cmath>

#define PLUGIN_NAME "Manual Tracking"
#define PLUGIN_FULL_VERSION "Manual Tracking by Maverick Tse (2017)"
#define TRACK_N 9
#define CHECK_N 3

TCHAR* track_label[] = { "width", "height", "RoT", "mode", "bufSz", "line_w", "R", "G", "B" };
int track_s[] = { 10, 10, 1, 1, 32, 1, 0, 0, 0 };
int track_e[] = { 1024, 1024, 120, 3, 8192, 10, 255, 255, 255 };
int track_d[] = { 100, 100, 12, 1, 8192, 3, 0, 255, 0 };

TCHAR* button_label[] = { "Save CSV", "Memory Clear", "HELP" };
int button_d[] = { -1, -1, -1 };


std::vector<char> write_buffer(track_d[2]);
std::ofstream hFile;

template <typename T>
class Angle {
private:
	T degree;
public:
	Angle() = default;
	T add(T delta)
	{
		T raw_angle = degree + delta;
		double clamped_angle = std::fmod(static_cast<double>(raw_angle), 360.0);
		degree = static_cast<T>(clamped_angle);
		return degree;
	}
	T get_degree()
	{
		return degree;
	}
	double get_radian()
	{
		double r = static_cast<double>(degree) * DirectX::XM_PI / 360.0;
		return r;
	}
};

bool is_recording{ false };
POINT mouse_loc{ 0,0 };
int mask_w{ 100 };
int mask_h{ 100 };
Angle<int> rotation{};
PIXEL_YC box_color{};



FILTER_DLL ManualTrackingPlugin =
{
	FILTER_FLAG_PRIORITY_LOWEST | FILTER_FLAG_EX_INFORMATION | FILTER_FLAG_MAIN_MESSAGE,
	0, 0,
	PLUGIN_NAME,
	TRACK_N,
	track_label,
	track_d,
	track_s, track_e,
	CHECK_N,
	button_label,
	button_d,
	func_proc,
	func_init,
	func_exit,
	func_update,
	func_WndProc,

	nullptr,
	nullptr,
	nullptr,
	0,

	PLUGIN_FULL_VERSION,
	nullptr,
	nullptr,
};

EXTERN_C __declspec(dllexport) FILTER_DLL* GetFilterTable(void)
{
	return &ManualTrackingPlugin;
}

BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	using namespace DirectX;
	if (!is_recording)
	{
		return FALSE;
	}
	if (!fp->exfunc->is_editing(fpip->editp))
	{
		return FALSE;
	}
	if (!fp->exfunc->is_filter_active(fp))
	{
		return FALSE;
	}
	// take a snapshot of global data
	auto mx = mouse_loc.x;
	auto my = mouse_loc.y;
	double radangle = rotation.get_radian();
	int degangle = rotation.get_degree();
	int mw = mask_w;
	int mh = mask_h;
	// Generate transformation matrix
	XMVECTOR Scaling = XMVectorSet(1.0, 1.0, 1.0, 0);
	XMVECTOR RotationOrigin = XMVectorSet(0.0, 0.0, 0.0, 1);
	float Rotation = static_cast<float>(radangle);
	//XMVECTOR Translation = XMVectorSetInt(static_cast<int>(mx), static_cast<int>(my), 0, 0);
	XMVECTOR Translation = XMVectorSet(static_cast<float>(mx), static_cast<float>(my), 0.0, 0.0);
	XMMATRIX TransformMatrix = XMMatrixAffineTransformation2D(Scaling, RotationOrigin, Rotation, Translation);
	// Generate bounding box pixels coordinates
	std::vector<XMFLOAT2> box_pixel_origin{};
	// XXXXXXXXXXX  (-w/2, -h/2) -> (-w/2 +w, -h/2)
	// XXXXXXXXXXX  (-w/2, -h/2 +l) -> (-w/2 +w, -h/2 +l)
	// II       II  L: (-w/2, -h/2 +l+1) -> (-w/2 +l, h/2 -l-1)
	// II       II  
	// II   o   II  R: (-w/2 + w-l, -h/2 +l+1) -> (-w/2 +w, h/2 -l-1)
	// II       II
	// II       II
	// XXXXXXXXXXX  (-w/2, h/2 -l) -> (-w/2 +w, h/2 -l)
	// XXXXXXXXXXX  (-w/2, h/2) -> (-w/2 +w, h/2)
		
	int w = mw;
	int h = mh;
	int l = fp->track[5];
	// Add top line
	for (int y = -h / 2; y <= (l - h / 2); ++y)
	{
		for (int x = -w / 2; x <= w - w / 2; ++x)
		{
			box_pixel_origin.push_back(XMFLOAT2{ static_cast<float>(x), static_cast<float>(y) });
		}
	}
	// Add bottom line
	for (int y = h / 2 - l; y <= h / 2; ++y)
	{
		for (int x = -w / 2; x <= w - w / 2; ++x)
		{
			box_pixel_origin.push_back(XMFLOAT2{ static_cast<float>(x), static_cast<float>(y) });
		}
	}
	// Add left line
	for (int y = 1 + l - h / 2; y <= h / 2 - l - 1; ++y)
	{
		for (int x = -w / 2; x <= l - w / 2; ++x)
		{
			box_pixel_origin.push_back(XMFLOAT2{ static_cast<float>(x), static_cast<float>(y) });
		}
	}
	// Add right line
	for (int y = 1 + l - h / 2; y <= h / 2 - l - 1; ++y)
	{
		for (int x = w - l - w / 2; x <= w - w / 2; ++x)
		{
			box_pixel_origin.push_back(XMFLOAT2{ static_cast<float>(x), static_cast<float>(y) });
		}
	}

	//Affine transform
	std::vector<XMFLOAT2> transformed_box(box_pixel_origin.size());
	XMVector2TransformCoordStream(&transformed_box[0], sizeof(XMFLOAT2), &box_pixel_origin[0], sizeof(XMFLOAT2), transformed_box.size(), TransformMatrix);

	// Draw box
	for (XMFLOAT2 px : transformed_box)
	{
		int ix = static_cast<int>(px.x);
		int iy = static_cast<int>(px.y);
		// remove outliers
		if (ix < 0 || iy < 0) continue;
		if (ix > fpip->w || iy > fpip->h) continue;
		// Set color at correct place
		PIXEL_YC* yuv = fpip->ycp_edit + iy* fpip->max_w + ix;
		yuv->y = box_color.y;
		yuv->cb = box_color.cb;
		yuv->cr = box_color.cr;
	}

	// Log data
	// frame, x, y, width, height, angle(degree)
	hFile << fpip->frame << "," << static_cast<int>(mx) << "," << static_cast<int>(my) << "," << mw << "," << mh << "," << degangle << std::endl;
	// refresh frame
	return TRUE;
}


BOOL func_init(FILTER *fp)
{
	static_assert(sizeof(track_label)/sizeof(track_label[0]) == TRACK_N, "Slider label count mismatch");
	static_assert(sizeof(track_s) / sizeof(track_s[0]) == TRACK_N, "Slider start value count mismatch");
	static_assert(sizeof(track_e) / sizeof(track_e[0]) == TRACK_N, "Slider end value count mismatch");
	static_assert(sizeof(track_d) / sizeof(track_d[0]) == TRACK_N, "Slider defualt value count mismatch");

	static_assert(sizeof(button_label) / sizeof(button_label[0]) == CHECK_N, "Button label count mismatch");
	static_assert(sizeof(button_d) / sizeof(button_d[0]) == CHECK_N, "Button default count mismatch");

	PIXEL default_picker{ static_cast<unsigned char>(track_d[8]), static_cast<unsigned char>(track_d[7]), static_cast<unsigned char>(track_d[6]) };
	fp->exfunc->rgb2yc(&box_color, &default_picker, 1);

	hFile.rdbuf()->pubsetbuf(&write_buffer.front(), write_buffer.size());
	hFile.open("temp_track_data.csv", std::ofstream::app | std::ofstream::ate | std::ofstream::out);
	return TRUE;
}

BOOL func_exit(FILTER *fp)
{
	hFile.close();
	return TRUE;
}

BOOL func_update(FILTER *fp, int status)
{
	if (status == FILTER_UPDATE_STATUS_TRACK + 0) //width
	{
		mask_w = fp->track[0];
	}
	if (status == FILTER_UPDATE_STATUS_TRACK + 1) //height
	{
		mask_h = fp->track[1];
	}
	// Just handle a change of buffer size
	if (status == FILTER_UPDATE_STATUS_TRACK + 4)
	{
		hFile.close();
		write_buffer.resize(fp->track[2]);
		hFile.rdbuf()->pubsetbuf(&write_buffer.front(), write_buffer.size());
		hFile.open("temp_track_data.csv", std::ofstream::app | std::ofstream::ate | std::ofstream::out);
	}
	if ((status >= FILTER_UPDATE_STATUS_TRACK + 6) && (status <= FILTER_UPDATE_STATUS_TRACK + 8))
	{
		PIXEL picker{ static_cast<unsigned char>(fp->track[8]), static_cast<unsigned char>(fp->track[7]), static_cast<unsigned char>(fp->track[6]) };
		fp->exfunc->rgb2yc(&box_color, &picker, 1);
	}

	return FALSE;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{

	if (message == WM_FILTER_MAIN_MOUSE_UP)
	{
		
		if (fp->exfunc->is_filter_active(fp)) // just after a click
		{
			/*int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			hFile << fp->exfunc->get_frame(editp) << "," << x << "," << y << std::endl;
			hFile.flush();*/
			mouse_loc.x = GET_X_LPARAM(lparam);
			mouse_loc.y = GET_Y_LPARAM(lparam);
			is_recording = !is_recording; // flip state
			if (is_recording)
			{
				SetWindowText(fp->hwnd, "Recording...");
			}
			else
			{
				SetWindowText(fp->hwnd, "Recording stopped");
			}
			
		}
		return TRUE;
	}

	if (message == WM_FILTER_MAIN_MOUSE_MOVE)
	{
		if ((fp->exfunc->is_filter_active(fp)) && (is_recording)) // ending click
		{
			/*int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			hFile << fp->exfunc->get_frame(editp) << "," << x << "," << y << std::endl;*/
			mouse_loc.x = GET_X_LPARAM(lparam);
			mouse_loc.y = GET_Y_LPARAM(lparam);
		}
		return TRUE;
	}

	//if (message == WM_FILTER_MAIN_MOUSE_WHEEL)
	//{
	//	if (fp->exfunc->is_filter_active(fp))
	//	{
	//		auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
	//		int angle_delta = (static_cast<int>(zDelta) / 120) * fp->track[2];
	//		rotation.add(angle_delta);
	//	}
	//}

	if (message == WM_FILTER_MAIN_KEY_DOWN)
	{
		if (fp->exfunc->is_filter_active(fp))
		{
			if (wparam == VK_UP)
			{
				int repeat = static_cast<int>(LOWORD(lparam));
				int angle_delta = fp->track[2] * repeat;
				rotation.add(angle_delta);
				return TRUE;
			}
			if (wparam == VK_DOWN)
			{
				int repeat = static_cast<int>(LOWORD(lparam));
				int angle_delta = fp->track[2] * repeat * -1;
				rotation.add(angle_delta);
				return TRUE;
			}
			
		}
	}
	// Button events
	if (message == WM_COMMAND)
	{
		if (wparam == MID_FILTER_BUTTON + 0) //CSV
		{
			char filename[MAX_PATH] = { 0 };
			if (fp->exfunc->dlg_get_save_name(filename, "Comma-Separated Values (*.csv)| *.csv", "raw_tracking_data.csv"))
			{
				hFile.close();
				if (CopyFile("temp_track_data.csv", filename, FALSE))
				{
					MessageBox(fp->hwnd, "Saved!", "Save tracking data", MB_OK);
				}
				hFile.open("temp_track_data.csv", std::ofstream::app | std::ofstream::ate | std::ofstream::out);
			}
		}

		//if (wparam == MID_FILTER_BUTTON + 1) //EXO
		//{
		//	MessageBox(hwnd, "ToDo: Save as EXO", "TODO", MB_OK|MB_ICONINFORMATION);
		//}

		if (wparam == MID_FILTER_BUTTON + 1) // MEM CLEAR
		{
			hFile.flush();
			hFile.close();
			hFile.open("temp_track_data.csv", std::ofstream::out | std::ofstream::trunc);
			hFile.close();
			hFile.open("temp_track_data.csv", std::ofstream::out | std::ofstream::app | std::ofstream::ate);
			MessageBox(hwnd, "Cached Data Deleted!", "Memory Clear", MB_OK | MB_ICONINFORMATION);
		}

		if (wparam == MID_FILTER_BUTTON + 2) // Help
		{
			MessageBox(hwnd, 
R"(
Manual Tracking Plugin for AviUtl
copyrighted @MaverickTse YM (2017) Hong Kong
==========================================
This is a plugin to track your hated object
in a video using the BEST Neural Network...
       >>> Your Brain! <<<
In essence, this is simply a mouse-cursor
coordinate logging plugin.

How To Use
==========================================
0. DISABLE the UP and DOWN shortcut keys
 in AviUtl if you want to rotate the bounding box
1. Load a video
2. Activate the plugin
3. Set a rough width and height for the
 bounding box
4. Click on the video to start logging
5A. Move your mouse so that the box encloses the target
5B. Press and Hold the RIGHT arrow key to move forward AND
5C. Move your mouse accordingly to follow target movement
5D. [Optional] Press the UP or DOWN key to Rotate the box
5E. Left click again on the main screen to stop logging
6. If you mess up on scene change, just left-click, stop
 recording, than move back a few frames, left-click again
 to resume recording.
7. Make sure recording has stopped
8. Click [Save CSV] and save the data somewhere
9. Click [Clear Memory] to purge all logged data
10. Starts a new tracking session

CSV Data Format
=========================================
The saved CSV file is title-less and have the 
following fields:
Frame no., x, y, width, height, rotation

Attention
=========================================
- Frame no. in a CSV is NOT unique!
- This plugin has no data clean-up feature
- This plugin does not generate EXO file!

Parameters
=========================================
width: width of bounding box [logged data]
height: height of bounding box [logged data]
RoT: How many degree the box turn when you
press UP or DOWN key once. Aka. Rate-of-Turn
mode: Not Used currently
bufSz: Size of write buffer
reduce this if you are really lacking RAM
line_w: line thickness of bounding box [visual cue]
RGB: color of bounding box [visual cue]
)"
, "Instructions", MB_OK | MB_ICONINFORMATION);
		}
	}
	return FALSE;
}
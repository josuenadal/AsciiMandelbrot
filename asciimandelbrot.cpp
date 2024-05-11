#include <iostream>
#include <math.h>
#include <unistd.h>
#include <iomanip>
#include <fstream>
#include <vector>
#include <gmp.h>
#include <gmpxx.h>
#include <ncurses.h>
#include <mpreal.h>
#include <thread>
#include <atomic>
#include <chrono>

using mpfr::mpreal;

double precision = 8;
const int THREAD_COUNT = 85;

std::atomic<bool> thread_keep_alive = false;
std::atomic<bool> pause_thread_flag;
std::atomic<bool> stop_running_threads_flag;
std::atomic<bool> kill_threads_flag;

// Screen Buffer Size in Character width/height
unsigned long int buffer_width = 0;
unsigned long int buffer_height = 0;

unsigned long int window_width = 0;
unsigned long int window_height = 0;

int input_timeout_ms;

// #define buffer_width 161
// #define buffer_height 45

//Shading array
const char* shade_chars = " .,-~o:;*=><!?#$@";

//Desired Focus
mpreal mpf_real_coordinate(precision);
mpreal mpf_imag_coordinate(precision);

//The area calculated for (the continuous plane)
mpreal mpf_real_min(precision);
mpreal mpf_real_max(precision);

mpreal mpf_imag_min(precision);
mpreal mpf_imag_max(precision);

//For calculating the current size of the real plane
mpreal mpf_width(precision);
mpreal mpf_height(precision);

//Zoom Factor - The area gets multiplied by this in order to shrink
mpreal mpf_zoom_factor(precision);

//initial number of iterationss
long int maxIterations;

//This number will go up after every frame, making the calculation depth deeper as the area calculated gets smaller.
float iter_factor = 0;

//Time to sleep for after every frame.
double sleep_time;

//Frame counter
long int frame_counter = 0;

//must be less than the same length of the shade_chars array
int char_count = 0;

//Use a file stream not attached to a file in order to get quick output
std::fstream bufferdump{};

size_t buffer_size;

//Actual buffer that will contain the graphics.
char * buffer;

const int screen_chunks = 14;

const int chunk_array_size = screen_chunks * screen_chunks;

bool&& val = false;
std::atomic<bool> screen_area_completion_index[chunk_array_size] = {val};

void reset_specific_chunks(int x_1, int x_2, int y_1, int y_2);

//Output some useful information about the map and program data.
void output_status_bar()
{
	int a_t = 0;
	for(int i = 0; i < chunk_array_size; i++)
	{
		if(screen_area_completion_index[i] == val)
		{
			a_t++;	
		}
	}
	// std::cout << "\nzoom_factor = " << mpf_zoom_factor << "\n";
	// std::cout << "real coord = " << mpf_real_coordinate << "\nimag coord = " << mpf_imag_coordinate << "\n";
	// std::cout << "real_min = " << mpf_real_min << " real_max = " << mpf_real_max << "\n";
	// std::cout << "imag_min = " << mpf_imag_min << " imag_max = " << mpf_imag_max << "\n";
	// std::cout << "frame counter = " << frame_counter << " Iterations: " << maxIterations << std::endl;
	printw("\nzoom_factor = %s\n", mpf_zoom_factor.toString().c_str());    
	printw("real coord = %s\nimag coord = %s\n", mpf_real_coordinate.toString().c_str(), mpf_imag_coordinate.toString().c_str());
	printw("real_min = %s real_max = %s\n", mpf_real_min.toString().c_str(), mpf_real_max.toString().c_str());
	printw("imag_min = %s imag_max = %s A_T = %d\n", mpf_imag_min.toString().c_str(), mpf_imag_max.toString().c_str(), a_t);
	printw("Iterations: %d frame counter = %d\n", maxIterations, frame_counter);
	printw("buffer_width: %d buffer_height = %d", buffer_width, buffer_height);
}
void sleep(double seconds)
{
	usleep(seconds*1000000);
}
void sleep_if_set()
{
	if(sleep_time)
	{
		sleep(sleep_time);
	}
}

//For ease of buffer access
#define coord(x,y) ((x)+(buffer_width)*(y))

//The scale is the relation between the actual span of the plane and the span of the buffer.
//It is calculated once for each frame.
mpreal mpf_width_scale;
void set_width_scale()
{
	mpf_width_scale = mpf_width / buffer_width;
}
mpreal mpf_height_scale;
void set_height_scale()
{
	mpf_height_scale = mpf_height / buffer_height;
}

//Converts to the buffer value into the actual point value.
//Maps from discrete buffer values to a point on the continuous plane.
mpreal map_horz_buffer_to_plane(int buffer_x)
{	
	return mpf_real_min + buffer_x * mpf_width_scale;
}
mpreal map_vert_buffer_to_plane(int buffer_y)
{
	return mpf_imag_min + buffer_y * mpf_height_scale;
}

//The half zoom is used to avoid repetitive divisions when zooming in.
mpreal mpf_half_zoom(precision);
void set_half_zoom_factor()
{
	mpf_half_zoom = mpf_zoom_factor * 0.5;
}

//Get the real sizes of the plane.
mpreal get_height()
{
	return mpf_imag_max - mpf_imag_min;
}
mpreal get_width()
{
	return mpf_real_max - mpf_real_min;
}


int chunk_width = std::floor(buffer_width / screen_chunks);
int chunk_height = std::floor(buffer_height / screen_chunks);
//This is used for panning. The translation distance is set before hand. 
mpreal mpf_transl_x;
mpreal mpf_transl_y;
bool transl_set_for_zlevel;
void set_translation_distance()
{
	if(transl_set_for_zlevel == false)
	{
		mpf_transl_x = chunk_width * mpf_width_scale;
		mpf_transl_y = chunk_height * mpf_height_scale;
		transl_set_for_zlevel = true;
	}
}
void move_up()
{
	// int vert_chunk = std::floor(buffer_height / screen_chunks);
	//memset(reinterpret_cast<void*>(buffer), ' ', vert_chunk * buffer_width);
	// memmove(buffer + vert_chunk * buffer_width, buffer, buffer_size);
	mpf_imag_min -= mpf_transl_y;
	mpf_imag_max += mpf_transl_y;
	mpf_imag_coordinate -= mpf_transl_y;
	// reset_specific_chunks(1, screen_chunks, 1, 1);
}
void move_down()
{
	// int vert_chunk = std::floor(buffer_height / screen_chunks);
	//memset(reinterpret_cast<void*>(buffer), ' ', vert_chunk * buffer_width);
	// memmove(buffer, buffer + vert_chunk * buffer_width, buffer_size - vert_chunk * buffer_width);
	mpf_imag_min += mpf_transl_y;
	mpf_imag_max -= mpf_transl_y;
	mpf_imag_coordinate += mpf_transl_y;
	// reset_specific_chunks(1, screen_chunks, screen_chunks, screen_chunks);
}
void move_left()
{
	mpf_real_min -= mpf_transl_x;
	mpf_real_max += mpf_transl_x;
	mpf_real_coordinate -= mpf_transl_x;
}
void move_right()
{
	mpf_real_min += mpf_transl_x;
	mpf_real_max -= mpf_transl_x;
	mpf_real_coordinate += mpf_transl_x;
}

mpreal mpf_zoom_out_factor;
void set_zoom_out_factor()
{
	mpf_zoom_out_factor = 1 - mpf_zoom_factor;
}
void zoom_out()
{
	mpreal half_width, half_height;

	half_width = (mpf_width + mpf_width * mpf_zoom_out_factor) * 0.5;
	mpf_real_min =  mpf_real_coordinate - half_width;
	mpf_real_max = mpf_real_coordinate + half_width;

	half_height = (mpf_height + mpf_height * mpf_zoom_out_factor) * 0.5;
	mpf_imag_min = mpf_imag_coordinate - half_height;
	mpf_imag_max = mpf_imag_coordinate + half_height;

	mpf_width = half_width + half_width;
	mpf_height = half_height + half_height;
	transl_set_for_zlevel = false;
}

void zoom()
{
	//Zooming is done by reducing the area calculated by zoom factor, then adding each 
	//half of the new area to each side of the coordenate. Thus always zooming around the chosen coordinate.
	mpreal half_width, half_height;

	//I use half of the factor to immediately get the half width with only one multiplication.
	//That way i do not have to multiply by the zoom factor then divide by 2. In order to-
	//add each half to each side of the coordinate.
	half_width = mpf_width * mpf_half_zoom;
	mpf_real_min =  mpf_real_coordinate - half_width;
	mpf_real_max = mpf_real_coordinate + half_width;

	half_height = mpf_height * mpf_half_zoom;
	mpf_imag_min = mpf_imag_coordinate - half_height;
	mpf_imag_max = mpf_imag_coordinate + half_height;

	mpf_width = half_width + half_width;
	mpf_height = half_height + half_height;
	transl_set_for_zlevel = false;
}

int mandelbrot(mpreal realc, mpreal imaginaryc)
{
	int iter_count = 0;

	mpreal zx(precision), zy(precision);
	zx = "0";
	zy = "0";

	mpreal xsqr(precision), ysqr(precision);
	xsqr = "0";
	ysqr = "0";

	while(iter_count < maxIterations && xsqr + ysqr < 4.0)
	{
	    zy *= zx;
	    zy += zy + imaginaryc;
	    zx = xsqr - ysqr + realc;
	    xsqr = zx * zx;
	    ysqr = zy * zy;
		iter_count++;
	}
	
	// std::complex<mpreal> coordenate(realc, imaginaryc);
	// std::complex<mpreal> z(0,0);
	// //Calculate values of each point until it reaches max iteration
	// while(abs(z) < 2 && iter_count < maxIterations)
	// {
	// 	z = z * z + coordenate;
	// 	iter_count++;
	// }

	return iter_count;
}

void center_around_set_coords()
{
	mpreal half_width, half_height;

	half_width = mpf_width * 0.5;
	mpf_real_min =  mpf_real_coordinate - half_width;
	mpf_real_max = mpf_real_coordinate + half_width;

	half_height = mpf_height * 0.5;
	mpf_imag_min = mpf_imag_coordinate - half_height;
	mpf_imag_max = mpf_imag_coordinate + half_height;

	transl_set_for_zlevel = false;
}

void set_iterations()
{
	timeout(-1);
	echo();
	char* input = new char[160];

	mvprintw(buffer_height, 0, "Set iterations.");
	refresh();
	move(buffer_height+6,12);
	clrtoeol();
	getstr(input);
	try
	{
		maxIterations = std::stoi(input);
	}
	catch(...)
	{
		mvprintw(buffer_height, 0, "Bad number try again.");
		refresh();
		sleep(1.0);
	}

	noecho();
    
    timeout(input_timeout_ms);
}

void set_coords()
{
    timeout(-1);
	echo();
	char* input = new char[160];

	mvprintw(buffer_height, 0, "Set real coordinate.");
	refresh();
	move(buffer_height+2,13);
	clrtoeol();
	getstr(input);
	mpf_real_coordinate = input;	

	mvprintw(buffer_height, 0, "Set imaginary coordinate.");
	refresh();
	move(buffer_height+3,13);
	clrtoeol();
	getstr(input);
	mpf_imag_coordinate = input;

	noecho();
	center_around_set_coords();
    
    timeout(input_timeout_ms);
}

char get_shade(int iter)
{
	if(iter == maxIterations)
	{
		return ' ';
	}
	return shade_chars[iter % char_count];
}

void iterate_each_buffer_point()
{
	set_height_scale();
	set_width_scale();

    for (int y = 0; y < buffer_height; y++)
    {
        for (int x = 0; x <= buffer_width; x++)
        {
        	buffer[coord(x,y)] = get_shade( mandelbrot( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
        }
    }
}

std::vector<std::thread> threads(THREAD_COUNT);

struct thread_params
{
	int horz_min;
	int horz_max;
	int vert_min;
	int vert_max;
};

std::vector<thread_params> thr_params;

void thread_loop();

int cap_height(int amt)
{
	if(amt > buffer_height)
	{
		return buffer_height;
	}
	return amt;
}

int cap_width(int amt)
{
	if(amt > buffer_width)
	{
		return buffer_width;
	}
	return amt;
}

//Divide the screen buffer into Screen_chunks sizes both vertically and horizontally.
//Account for the discrepancies that may arise when dividing. Remainder will always go to last chunks.
std::vector<thread_params> get_screen_chunks()
{
	std::vector<thread_params> vec;
	thread_params tmp_params;

	int horz_span = chunk_width;
	int vert_span = chunk_height;

	int horz_add = 0;
	int vert_add = 0;

	bool horz_adj = false;
	if(buffer_width % horz_span > 0)
	{
		horz_adj = true;
	}

	bool vert_adj = false;
	if(buffer_height % vert_span > 0)
	{
		vert_adj = true;
	}
	
	int horz_remainder = 0;
	int vert_remainder = 0;
	int col = 1;
	int row = 1;

	for (int cur_vert_span = 0; cur_vert_span < buffer_height; cur_vert_span += vert_span)
    {
    	row = 1;
    	for (int cur_horz_span = 0; cur_horz_span < buffer_width; cur_horz_span += horz_span)
        {
			horz_add = horz_span;
			vert_add = vert_span;

        	if(horz_adj && row == screen_chunks)
        	{
        		//If you're at the last column or the last row 
        		horz_span = buffer_width - (cur_horz_span);
        	}
        	if(vert_adj && col == screen_chunks)
        	{
        		vert_span = buffer_height - (cur_vert_span);
        	}

			tmp_params = {	cur_horz_span, 
							cap_width(cur_horz_span + horz_span),
							cur_vert_span, 
							cap_height(cur_vert_span + vert_span),
						};

			vec.push_back(tmp_params);
			row++;
        }
		col++;
    }

	return vec;
}


//This function was for selective redrawing of screen chunks to avoid recalculating
//the whole screen when moving the camera. It has a few bugs.
//Namely the bug is that this mechanism depends on moving the buffer and drawing the
//only the  newly exposed area, but if you move the camera too fast it moves the unfinished 
//area across the screen. Leaving trails all over the screen.
void reset_specific_chunks(int x_1, int x_2, int y_1, int y_2)
{
	set_height_scale();
	set_width_scale();

	int i = 0;
    for (int y = 1; y <= screen_chunks; y++)
    {
    	for (int x = 1; x <= screen_chunks; x++)
        {
        	if(x >= x_1 && x <= x_2 && y >= y_1 && y <= y_2)
			{
				//Set indexes selected by params to false so threads will redraw selected area
				screen_area_completion_index[i].store(val);
			}
			i++;
        }
    }
}

//Set all screen chunks to false so whole screen is re-rendered.
void reset_chunks_index()
{
	pause_thread_flag.store(true);
	stop_running_threads_flag.store(true);
	for(int i = 0; i < chunk_array_size; i++)
	{
		screen_area_completion_index[i].store(val);
	}
	stop_running_threads_flag.store(false);
	pause_thread_flag.store(false);

	memset(reinterpret_cast<void*>(buffer), ' ', buffer_size);
}

//Display threads was supposed to leave one unrendered char between every screen chunk;
//currently not working.
const bool display_threads = false;
void calculate_buffer_area_threaded(int x_1, int x_2, int y_1, int y_2)
{
	
	int d_t = 0;
	if(display_threads)
	{
		//If this is subtracted from the loop you can see the boundaries between the thread calculation areas. 
		d_t = -1;
	}
	set_height_scale();
	set_width_scale();

    for (int y = y_1; y < y_2-d_t; y++)
    {
    	for (int x = x_1; x < x_2-d_t; x++)
        {
        	if(stop_running_threads_flag | kill_threads_flag){return;}
        	buffer[coord(x,y)] = get_shade( mandelbrot( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
        }
    }
}

//This will draw spaces to each screen chunk, put this before a 
//call to calculate_buffer_area_threaded for visualizing drawing.
void set_area_to_space(int x_1, int x_2, int y_1, int y_2)
{
	int d_t = 0;
	if(display_threads)
	{
		//If this is subtracted from the loop you can see the boundaries between the thread calculation areas. 
		d_t = -1;
	}
	set_height_scale();
	set_width_scale();

    for (int y = y_1; y < y_2-d_t; y++)
    {
    	for (int x = x_1; x < x_2-d_t; x++)
        {
        	if(stop_running_threads_flag | kill_threads_flag){return;}
        	buffer[coord(x,y)] = ' ';
        }
    }
}

//Used to permanently stop all threads.
void kill_threads()
{
	kill_threads_flag.store(true);
}

//Used to stop threads that are currently drawing the screen.
void stop_running_threads()
{
	stop_running_threads_flag.store(true);
}

//Re-activates screen drawing.
void activate_threads()
{
	stop_running_threads_flag.store(false);
}

//This is where threads hang out. Eternally looping in here.
//They will always check for chunks to draw, unless paused or stopped.
void thread_loop()
{
	do
	{
		int chkd = 0;
		for(int i = 0; i < chunk_array_size; i++)
		{
			if(screen_area_completion_index[i].load() == val)
			{	
				screen_area_completion_index[i].store(true);
				// set_area_to_space(thr_params[i].horz_min, thr_params[i].horz_max, thr_params[i].vert_min, thr_params[i].vert_max);
				calculate_buffer_area_threaded(thr_params[i].horz_min, thr_params[i].horz_max, thr_params[i].vert_min, thr_params[i].vert_max);
			}
			// else
			// {
			// 	chkd++;
			// }

        	if(stop_running_threads_flag | kill_threads_flag)
			{
				return;
			}

			// if(chkd == chunk_array_size - 1)
			// {
			// 	pause_thread_flag.store(true);
			// }

			//If pause_thread_flag is set, threads will loop here to prevent doing work
			//while resetting screen_area_completion_index.
			// while(pause_thread_flag == true)
			// {
			// 	std::this_thread::yield();
			// }

		}
	}while(thread_keep_alive == true);
}

//In case you want to join the threads.
void join_threads()
{
	for(int i = 0; i < THREAD_COUNT; i++)
	{
		threads[i].join();
	}
}

//Create all the threads and send em to the loop.
void spawn_threads()
{
	reset_chunks_index();

	for(int i = 0; i < THREAD_COUNT; i++)
	{
		threads[i] = std::thread(thread_loop);
	}

	join_threads();
}

//For calculating the screen chunks that the threads will each draw.
void set_screen_chunks()
{
	stop_running_threads();
	thr_params = get_screen_chunks();
	activate_threads();
}

//Erases the screen.
void erase_buffer()
{
	// std::cout << std::flush;
    // std::system("clear");
    //clear();
    erase();
}

//Draw the screen.
void display_buffer()
{
    for (int y = 0; y < buffer_height; y++)
    {
        for (int x = 0; x < buffer_width; x++)
        {
        	addch(buffer[coord(x,y)]);
        }
        addch('\n');
    }
}

//Loop for drawing a frame to the screen.
void display()
{
	erase();
	display_buffer();
	output_status_bar();
	refresh();
}

//Function for initializing the screen size, also used for resizing.
//Resize errors are either caused by too big of a buffer
//or because the mapping function breaks. Still working on it.
void set_screen_size()
{
	getmaxyx(stdscr, window_height, window_width);
	
	//Set buffer dimensions.
	buffer_height = window_height - 8;
	buffer_width = window_width - 1;
	buffer_size = (buffer_height) * (buffer_width);
	buffer = new char[buffer_size];
	memset(reinterpret_cast<void*>(buffer), ' ', buffer_size);
}


void on_screen_resize()
{
	set_screen_size();
	//Get thread init only needs to be run whenever the screen size changes.
	//As this is the function that divides the screen buffer into the threads
	set_screen_chunks();
	spawn_threads();

}

//Check if screen size has changed.
void check_screen_size()
{
	unsigned long int tmp_x, tmp_y;
	tmp_x = 0;
	tmp_y = 0;

	printw("set");

	getmaxyx(stdscr, tmp_y, tmp_x);

	if(tmp_x != window_width | tmp_y != window_height)
	{
		on_screen_resize();	
	}
}

int main(int argc, char *argv[])
{
	int c;

	//Ncurses init
	initscr();
	keypad(stdscr, TRUE);
	clear();
	noecho();

	std::ios_base::sync_with_stdio(false);
	mpreal::set_default_prec(mpfr::digits2bits(precision));
	std::cout << std::setprecision(precision);
  	nodelay(stdscr, TRUE);

	//Initialize global variables
	set_screen_size();
	pause_thread_flag = true;
	input_timeout_ms = -1;
	transl_set_for_zlevel = false;
	mpf_height_scale = 0;
	mpf_width_scale = 0;
	mpf_transl_x = 0;
	mpf_transl_y = 0;
	iter_factor = 0;
	char_count = 17;
	sleep_time = 0;
	maxIterations = 25;
	mpf_real_coordinate = "0";
	mpf_imag_coordinate = "0";
	mpf_zoom_factor = "0.9";
	mpf_real_min = "-3";
	mpf_real_max = "3";
	mpf_imag_min = "-2";
	mpf_imag_max = "2";

	mpf_width = get_width();
	mpf_height = get_height();
    
    timeout(input_timeout_ms);

	set_half_zoom_factor();
	set_zoom_out_factor();
	set_screen_chunks();
	spawn_threads();

	set_translation_distance();

	sleep(1.0);

	while(1)
	{
		display();

		c = getch();

		switch(c)
		{			
			case 10:	//10 is enter on normal keyboard
				zoom();
				set_translation_distance();
				spawn_threads();
				break;
			case KEY_BACKSPACE:
				zoom_out();
				set_translation_distance();
				spawn_threads();
				break;
			case KEY_UP:
				move_up();
				spawn_threads();
				break;
			case KEY_DOWN:
				move_down();
				spawn_threads();
				break;
			case KEY_LEFT:
				move_left();
				spawn_threads();
				break;
			case KEY_RIGHT:
				move_right();
				spawn_threads();
				break;
			case 88: 	// uppercase X
			case 120: 	// lowercase X
				set_coords();
				spawn_threads();
				break;
			case 73:	//uppercase I
			case 105:	//lowercase i
				set_iterations();
				spawn_threads();
				break;
			case 113: //letter q, for quit
			case 27:  //escape key
				endwin();
				return 1;
				break;
			default:
			 	check_screen_size();
				mvprintw(buffer_height, 0,"Character pressed is = %3d Hopefully it can be printed as '%c'", c, c);
				// refresh();
				// getch();
				break;		
		}

		frame_counter++;
	}
	endwin();
	return 1;
}

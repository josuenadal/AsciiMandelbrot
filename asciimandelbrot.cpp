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

using mpfr::mpreal;

double precision = 128;

// Screen Buffer Size in Character width/height
unsigned long int buffer_width = 633;
unsigned long int buffer_height = 125;

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
int sleep_time;

//Frame counter
int frame_counter = 0;

//must be less than the same length of the shade_chars array
int char_count = 0;

//Use a file stream not attached to a file in order to get quick output
std::fstream bufferdump{};

size_t buffer_size;

//Actual buffer that will contain the graphics.
char * buffer;

void output_status_bar()
{
	// std::cout << "\nzoom_factor = " << mpf_zoom_factor << "\n";
	// std::cout << "real coord = " << mpf_real_coordinate << "\nimag coord = " << mpf_imag_coordinate << "\n";
	// std::cout << "real_min = " << mpf_real_min << " real_max = " << mpf_real_max << "\n";
	// std::cout << "imag_min = " << mpf_imag_min << " imag_max = " << mpf_imag_max << "\n";
	// std::cout << "frame counter = " << frame_counter << " Iterations: " << maxIterations << std::endl;
	printw("\nzoom_factor = %s\n", mpf_zoom_factor.toString().c_str());    
	printw("real coord = %s\nimag coord = %s\n", mpf_real_coordinate.toString().c_str(), mpf_imag_coordinate.toString().c_str());
	printw("real_min = %s real_max = %s\n", mpf_real_min.toString().c_str(), mpf_real_max.toString().c_str());
	printw("imag_min = %s imag_max = %s\n", mpf_imag_min.toString().c_str(), mpf_imag_max.toString().c_str());
	printw("frame counter = %d Iterations: %d", frame_counter, maxIterations);
}
void sleep(int seconds)
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

mpreal mpf_transl_x;
mpreal mpf_transl_y;
bool transl_set_for_zlevel;
void set_translation_distance()
{
	if(transl_set_for_zlevel == false)
	{
		mpf_transl_x = mpf_width * 0.125;
		mpf_transl_y = mpf_height * 0.125;
		transl_set_for_zlevel = true;
	}
}
void move_up()
{
	mpf_imag_min -= mpf_transl_y;
	mpf_imag_max += mpf_transl_y;
	mpf_imag_coordinate -= mpf_transl_y;
}
void move_down()
{
	mpf_imag_min += mpf_transl_y;
	mpf_imag_max -= mpf_transl_y;
	mpf_imag_coordinate += mpf_transl_y;
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
	move(buffer_height+6,30);
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
		sleep(1);
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

void iterate_buffer_on_x_range_c(int x_1, int x_2, char c)
{
	set_height_scale();
	set_width_scale();

    for (int y = 0; y < buffer_height; y++)
    {
        for (int x = x_1; x < x_2; x++)
        {
        	buffer[coord(x,y)] = c;
        }
    }
}

//This variable will make the threads display one less character, thus
//making them more noticeable.
bool display_threads;
int d_t;
void iterate_buffer_on_x_range(int x_1, int x_2)
{
	d_t = 0;
	if(display_threads)
	{
		d_t = -1;
	}
	set_height_scale();
	set_width_scale();

    for (int y = 0; y < buffer_height; y++)
    {
        for (int x = x_1; x < x_2-d_t; x++)
        {
        	buffer[coord(x,y)] = get_shade( mandelbrot( map_horz_buffer_to_plane(x), map_vert_buffer_to_plane(y)));
        }
    }
}

const int THREAD_COUNT = 16;

std::vector<std::thread> threads(THREAD_COUNT);

struct thread_params
{
	int min;
	int max;
};

std::vector<thread_params> get_thread_init_vec(int threads)
{
	std::vector<thread_params> vec;
	thread_params tmp_params;

	int span = std::floor(buffer_width / threads);
	int cur_span = 0;

	for(int i = 0; i < threads; i++)
	{
		if(i == threads-1)
		{	
			//When you are at the last thread, add the remainder.
			int remainder = buffer_width - (cur_span + span);
			tmp_params = {cur_span, cur_span + span + remainder};
		}
		else
		{
			tmp_params = {cur_span, cur_span + span};
		}
		vec.push_back(tmp_params);

		cur_span += span;
	}
	return vec;
}

std::vector<thread_params> thr_params;
void threaded_iterations()
{
	for(int i = 0; i < thr_params.size(); i++)
	{
		threads[i] = std::thread(iterate_buffer_on_x_range, thr_params[i].min, thr_params[i].max);
	}

    for (auto& th : threads) {
    	th.detach();
	}
}

void set_thread_params()
{
	thr_params = get_thread_init_vec(THREAD_COUNT);
}


void erase_buffer()
{
	// std::cout << std::flush;
    // std::system("clear");
    clear();
}

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

void display()
{
	clear();
	display_buffer();
	output_status_bar();
	refresh();
}

void set_screen_size()
{
	getmaxyx(stdscr, window_height, window_width);

	buffer_height = window_height - 8;
	buffer_width = window_width - 1;

	//Resize errors are either caused by too big of a buffer
	//Or because the mapping function breaks when 
	buffer_size = (buffer_height) * (buffer_width);
	buffer = new char[buffer_size];
	memset(reinterpret_cast<void*>(buffer), ' ', buffer_size);
}



void on_screen_resize()
{
	set_screen_size();
	//Get thread init only needs to be run whenever the screen size changes.
	//As this is the function that divides the screen buffer into the threads
	set_thread_params();
	threaded_iterations();
}

unsigned long int tmp_x, tmp_y;
void check_screen_size()
{
	tmp_x = 0;
	tmp_y = 0;

	printw("set");

	getmaxyx(stdscr, tmp_y, tmp_x);

	if(tmp_x != window_width | tmp_y != window_height)
	{
		printw("%d, %d", tmp_x, tmp_y);
		on_screen_resize();
		
	}
}

int main(int argc, char *argv[])
{
	int c;
	initscr();
	keypad(stdscr, TRUE);
	clear();
	noecho();

	set_screen_size();

	std::ios_base::sync_with_stdio(false);
	mpreal::set_default_prec(mpfr::digits2bits(precision));
	std::cout << std::setprecision(precision);

	input_timeout_ms = 35;
	
	transl_set_for_zlevel = false;
	mpf_height_scale = 0;
	mpf_width_scale = 0;
	mpf_transl_x = 0;
	mpf_transl_y = 0;

	iter_factor = 0;
	char_count = 17;
	sleep_time = 0;
	maxIterations = 1000;
	mpf_real_coordinate = "0";
	mpf_imag_coordinate = "0";
	mpf_zoom_factor = "0.8";
	mpf_real_min = "-3";
	mpf_real_max = "3";
	mpf_imag_min = "-2";
	mpf_imag_max = "2";

	mpf_width = get_width();
	mpf_height = get_height();
    
    timeout(input_timeout_ms);

	set_half_zoom_factor();
	set_zoom_out_factor();
	set_translation_distance();

	set_thread_params();
	threaded_iterations();

	display();

	sleep(1);

	while(1)
	{
		std::cout << "\n";

		display();

		c = getch();

		switch(c)
		{			
			case 10:	//10 is enter on normal keyboard
				zoom();
				set_translation_distance();
				threaded_iterations();
				break;
			case KEY_BACKSPACE:
				zoom_out();
				set_translation_distance();
				threaded_iterations();
				break;
			case KEY_UP:
				move_up();
				threaded_iterations();
				break;
			case KEY_DOWN:
				move_down();
				threaded_iterations();
				break;
			case KEY_LEFT:
				move_left();
				threaded_iterations();
				break;
			case KEY_RIGHT:
				move_right();
				threaded_iterations();
				break;
			case 88: 	// uppercase X
			case 120: 	// lowercase X
				set_coords();
				threaded_iterations();
				//move(0,0);
				break;
			case 73:	//uppercase I
			case 105:	//lowercase i
				set_iterations();
				threaded_iterations();
				break;
			case 113: //letter q, for quit
			case 27:  //escape key
				endwin();
				return 1;
				break;
			default:
				check_screen_size();
				// mvprintw(buffer_height, 0,"Charcter pressed is = %3d Hopefully it can be printed as '%c'", c, c);
				// refresh();
				// getch();
				break;		
		}

		frame_counter++;

		sleep_if_set();
	}
	endwin();
	// bufferdump.close();
}

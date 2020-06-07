/*

Copyright (C) 2018 by Richard Sandberg.

This is the Windows specific version of rsMetaFont.
No Unix version exists yet.

*/

#pragma once
#define TEX_STRING(s) 1



#include <cstdio>
#include <cassert>
#include <array>

#define array_size maxindex - minindex + 1
template<typename T, int minindex, int maxindex>
struct Array
{
	T& operator[](int x) {
		assert(x >= minindex && x <= maxindex);
		return _array[x - minindex];
	}

	T* get_c_str() {
		assert(array_size > 0);
		_array[array_size - 1] = T(0);
		return &_array[0];
	}
	static const int _minindex = minindex;
	static const int _maxindex = maxindex;
	std::array<T, array_size> _array;
};

// 2
#define banner "This is rsMETAFONT, Version 1.0 Windows" // printed when METAFONT starts

// 11
const int mem_max = 50000;
const int max_internal = 100;
const int buf_size = 500;
const int error_line = 72;
const int half_error_line = 42;
const int max_print_line = 79;
const int screen_width = 1200;//768;
const int screen_depth = 1200;//1024;
const int stack_size = 30;
const int max_strings = 2000;
const int string_vacancies = 8000;
const int pool_size = 32000;
const int move_size = 5000;
const int max_wiggle = 300;
const int gf_buf_size = 800;
const int file_name_size = 300;
const char pool_name[] = "mf.pool";
const int path_size = 300;
const int bistack_size = 785;
const int header_size = 100;
const int lig_table_size = 5000;
const int max_kerns = 500;
const int max_font_dimen = 50;

// 12
const int mem_min = 0;
const int mem_top = 50000;
const int hash_size = 2100;
const int hash_prime = 1777;
const int max_in_open = 6;
const int param_size = 150;


// 37
typedef int pool_pointer; // 0 .. pool_size, for variables that point into str_pool
typedef int str_number; // 0 .. max_strings, for variables that point into str_start
typedef unsigned char packed_ASCII_code; // 0 .. 255, elements of str_pool array



// 13
int bad; // is some constant wrong



//////////////////////////////////////////////
// System dependent addition for paths
Array<char, 1, file_name_size + 1> real_name_of_file;



const int tab = 011; // ASCII horizontal tab
const int form_feed = 014; // ASCII form-feed



// 16
#define _double(n) n = n + n

#define negate(n) n = -n

// 18
typedef unsigned char ASCII_code;

// 19
typedef unsigned char text_char; // The data type of characters in text files
const int first_text_char = 0; // Ordinal number of the smallest element of text_char
const int last_text_char = 255; // Ordinal number of the largest element of text_char

// 20
Array<ASCII_code, 0, 255> xord; // Specifies conversion of input characters
Array<text_char, 0, 255> xchr; // Specifies conversion of output characters


// 24
typedef unsigned char eight_bits; // Unsigned one-byte quantity
typedef FILE* alpha_file; // Files that contain textual data
typedef FILE* byte_file; // Files that contain binary data

// 25
Array<char, 1, file_name_size + 1> name_of_file;
//int name_length; // This many characters are actually relevant in name_of_file (the rest are blank) NOTE: We don't really follow this convention in C

// 29

Array<ASCII_code, 0, buf_size> buffer; // lines of characters being read
int first; // The first unused position in buffer
int last; // End of the line just input to buffer
int max_buf_stack; // Largest index used in buffer

// 31
alpha_file term_in = stdin; // the terminal as an input file
alpha_file term_out = stdout; // the terminal as an output file

// 35
#define loc cur_input.loc_field // location of first unread character in buffer

// 37
#define si(n) n //{convert from |ASCII_code| to |packed_ASCII_code|}
#define so(n) n //{convert from |packed_ASCII_code| to |ASCII_code|}


///////////////////////////////////////////////////////////////////////////
// System specific addition on windows

#define MAX_INPUT_CHARS 700
#define default_input_path ".;..;c:\\mf\\local\\lib"
#define MAX_OTH_PATH_CHARS 200
//#define default_font_path "c:\\tex\\fonts"
#define default_base_path ".;c:\\tex\\local\\tex"
#define default_pool_path ".;c:\\mf\\local\\tex"

#define edit_file input_stack[file_ptr]

pool_pointer ed_name_start;
int ed_name_length;
int edit_line;

char input_path[MAX_INPUT_CHARS] = default_input_path;
//char font_path[MAX_OTH_PATH_CHARS] = default_font_path;
char base_path[MAX_OTH_PATH_CHARS] = default_base_path;
char pool_path[MAX_OTH_PATH_CHARS] = default_pool_path;

const int no_file_path = 0;
const int input_file_path = 1;
const int read_file_path = 2;
//const int font_file_path = 3;
const int base_file_path = 4;
const int pool_file_path = 5;
///////////////////////////////////////////////////////////////////////////////



// 38
Array<packed_ASCII_code, 0, pool_size> str_pool; // the characters
Array<pool_pointer, 0, max_strings> str_start; // the starting pointers
pool_pointer pool_ptr; // first unused position in str_pool
str_number str_ptr; // number of the current string being created
pool_pointer init_pool_ptr; // the starting value of pool_ptr
str_number init_str_ptr; // the starting value of str_ptr
pool_pointer max_pool_ptr; // the maximum so far of pool_ptr
str_number max_str_ptr; // the maximum so far of str_ptr


// 39
#define length(num) (str_start[num + 1] - str_start[num]) // the number of characters in string number num

// 40
#define cur_length (pool_ptr - str_start[str_ptr])

// 42
// put ASCII_code thechar at the end of str_pool
#define append_char(thechar) do{\
	str_pool[pool_ptr] = thechar;\
	pool_ptr++;\
} while(0)

// make sure that pool hasn't overflowed
#define str_room(num) do{\
	if (pool_ptr + num > max_pool_ptr){\
		if (pool_ptr + num > pool_size)\
			overflow(/*pool size*/1072, pool_size - init_pool_ptr);\
		max_pool_ptr = pool_ptr + num;\
	}\
} while(0)

const int max_str_ref = 127; // "infinite" number of references

#define add_str_ref(num) do{\
	if (str_ref[num] < max_str_ref)\
		str_ref[num]++;\
} while(0)

Array<int, 0, max_strings> str_ref; // 0..max_str_ref


// 43
#define delete_str_ref(num) do{\
	if (str_ref[num] < max_str_ref)\
		if(str_ref[num] > 1) str_ref[num]--;\
		else flush_string(num);\
} while(0)

// 48

#define app_lc_hex(num) do {\
	l = num;\
	if(l < 10)\
		append_char(l + /*0*/48);\
	else\
		append_char(l - 10 + /*a*/97);\
} while(0)

// 50
#ifndef NO_INIT
alpha_file pool_file; // the string-pool file output by TANGLE
#endif


// 51
// NOTE: Add extra parameter if we should close the file
#define bad_pool(str, do_close) do {\
	puts(str);\
	if(do_close)\
		a_close(pool_file);\
	return false;\
} while(0)

// 54
const int no_print = 0; // selector setting that makes data disappear
const int term_only = 1; // printing is destined for the terminal only
const int log_only = 2; // printing is destined for teh transcript file only
const int term_and_log = 3; // normal selector setting
const int pseudo = 4; // special selector setting for show_context
const int new_string = 5; // printing is deflected to the string pool
const int max_selector = 5; // highest selector setting


alpha_file log_file; // transcript of METAFONT session
int selector; // 0..max_selector, where to print a message
unsigned char dig[23]; // 0..15, digits in a number being output
int tally; // the number of characters recently printed
int term_offset; // 0..max_print_line, the number of characters on the current terminal line
int file_offset; // 0..max_print_line, the number of characters on the current file line

Array<ASCII_code, 0, error_line> trick_buf; // circular buffer for pseudoprinting
int trick_count; // threshold for pseudoprinting
int first_count; // another variable for pseudoprinting


// 56

#define wterm_s(s) fputs(s, term_out)
#define wterm_c(s) fputc(s, term_out)
#define wterm_ln_s(s) do {fputs(s, term_out);fputc('\n',term_out);} while(0)
#define wterm_cr fputc('\n', term_out)

#define wlog_s(s) do{\
	fputs(s, log_file);\
} while(0)

#define wlog_c(s) do{\
	fputc(s, log_file);\
} while(0)

#define wlog_ln_s(s) do{\
	fputs(s, log_file);\
	fputc('\n', log_file);\
} while (0)

#define wlog_cr do{\
	fputc('\n',log_file);\
} while(0)


// 66
#define prompt_input(s) do{\
	wake_up_terminal(); print(s); term_input();\
} while(false) // prints a string and gets a line of input



// 68
const int batch_mode = 0;
const int nonstop_mode = 1;
const int scroll_mode = 2;
const int error_stop_mode = 3;

#define print_err(s) do{\
	if (interaction == error_stop_mode) wake_up_terminal();\
	print_nl(/*! */758); print(s);\
} while(0)

unsigned char interaction; // batch_mode..error_stop_mode, current level of interaction

// 71
const int spotless = 0; // history value when nothing has been amiss yet
const int warning_issued = 1; // history value when begin_diagnostic has been called
const int error_message_issued = 2; // history value when error has been called
const int fatal_error_stop = 3; // history value when termination was premature

bool deletions_allowed; // is it safe for error to call get_next
int history; // spotless..fatal_error_stop, has the source input been clean so far?
int error_count; // -1..100, the number of scrolled errors since the last statement ended


// 74
// sometimes there might be no help
#define help0 do{\
	help_ptr = 0;\
} while(0)

// use this with one help line
#define help1(s) do{\
	help_ptr = 1;\
	help_line[0] = s;\
} while(0)

// use this with two help lines
#define help2(s1,s2) do{\
	help_ptr = 2;\
	help_line[1] = s1;\
	help_line[0] = s2;\
} while(0)

// use this with three help lines
#define help3(s1,s2,s3) do{\
	help_ptr = 3;\
	help_line[2] = s1;\
	help_line[1] = s2;\
	help_line[0] = s3;\
} while(0) 

// use this with four help lines
#define help4(s1,s2,s3,s4) do{\
	help_ptr = 4;\
	help_line[3] = s1;\
	help_line[2] = s2;\
	help_line[1] = s3;\
	help_line[0] = s4;\
} while(0)

// use this with five help lines
#define help5(s1,s2,s3,s4,s5) do{\
	help_ptr = 5;\
	help_line[4] = s1;\
	help_line[3] = s2;\
	help_line[2] = s3;\
	help_line[1] = s4;\
	help_line[0] = s5;\
} while(0)

// use this with six help lines
#define help6(s1,s2,s3,s4,s5,s6) do{\
	help_ptr = 6;\
	help_line[5] = s1;\
	help_line[4] = s2;\
	help_line[3] = s3;\
	help_line[2] = s4;\
	help_line[1] = s5;\
	help_line[0] = s6;\
} while(0) 

Array<str_number, 0, 5> help_line; // helps for the next error
char help_ptr; // 0..6, the number of help lines present
bool use_err_help; // should err_help list be shown?
str_number err_help; // a string set up by errhelp


// 91
#define check_interrupt if(interrupt != 0) pause_for_instructions()

int interrupt; // should METAFONT pause for instructions?
bool OK_to_interrupt; // should interrupts be observed?

// 95
const int el_gordo = 017777777777; // 2^31 - 1, the largest value that METAFONT likes

// 96
#define half(n) (n) / 2


// 97
bool arith_error; // has arithmetic overflow occurred recently?

// 99
#define check_arith if(arith_error) clear_arith()

// 101
const int quarter_unit = 040000; // 2^14, represents 0.250000
const int half_unit = 0100000; // 2^15, represetns 0.50000
const int three_quarter_unit = 0140000; // 2*2^14, represents 0.75000
const int unity = 0200000; // 2^16, represents 1.00000
const int two = 0400000; // 2^17, represents 2.00000
const int three = 0600000; // 2^17+2^16, represents 3.00000
typedef int scaled; // This type is used for scaled integers
typedef int small_number; // 0.. 63, this type is self-explanatory

// 105
const int fraction_half = 01000000000; // 2^27, represents 0.50000000
const int fraction_one = 02000000000; // 2^28, represents 1.00000000
const int fraction_two = 04000000000; // 2^29, represents 2.00000000
const int fraction_three = 06000000000; // 3*2^28, represents 3.00000000
const int fraction_four = 010000000000; // 2^30, represents 4.00000000

typedef int fraction; // this type is used for scaled fractions

// 106
const int forty_five_deg = 0264000000; // 45*2^20, represents 45 deg
const int ninety_deg = 0550000000; // 90*2^20, represents 90 deg
const int one_eighty_deg = 01320000000; // 180*2^20, represents 180 deg
const int three_sixty_deg = 02640000000; // 360*2^20, represetns 360 deg

typedef int angle; // this type is used for scaled angles


// 117
#define return_sign(n) return n

// 129
Array<int, 0, 30> two_to_the; // powers of two
Array<int, 1, 28> spec_log; // special logarithms

// 137
Array<angle, 1, 26> spec_atan; // arctan 2^(-k) times 2^20*180/Pi

// 139
const int negate_x = 1;
const int negate_y = 2;
const int switch_x_and_y = 4;
const int first_octant = 1;
const int second_octant = first_octant + switch_x_and_y;
const int third_octant = first_octant + switch_x_and_y + negate_x;
const int fourth_octant = first_octant + negate_x;
const int fifth_octant = first_octant + negate_x + negate_y;
const int sixth_octant = first_octant + switch_x_and_y + negate_x + negate_y;
const int seventh_octant = first_octant + switch_x_and_y + negate_y;
const int eighth_octant = first_octant + negate_y;


// 144
fraction n_sin; // results computed by n_sin_cos
fraction n_cos;

// 148
Array<fraction, 0, 54> randoms; // the last 55 random values generated
int j_random; // 0..54, the number of unused randoms

// 149
#define next_random if (j_random == 0) new_randoms(); else j_random--

// 153
const int min_quarterword = 0; // Smallest allowable value in quarterword
const int max_quarterword = 255; // Largest allowable value in quarterword
const int min_halfword = 0; // Smallest allowable value in halfword
const int max_halfword = 65535;//1073741823; // Largest allowable value in halfword

typedef unsigned char quarterword; // min_quarterword..max_quarterword, 1/4 of a word, NOTE: This part assumes min_quarterword=0
typedef unsigned short halfword;//typedef int halfword; // min_halfword..max_halfword, 1/2 of a word, NOTE: This part assumes min_halfword=0

// 155
#define ho(n) n - min_halfword // to take a sixteen bit item from a halfword
#define qo(n) n - min_quarterword // to read eight bits from a quarterword
#define qi(n) n + min_quarterword // to store eight bits in a quarterword

// 156
#define sc an_int // Scaled data is equivalent to integer

typedef struct two_halves
{
	halfword rh;
	union {
		halfword lh;
		struct {
			quarterword b0;
			quarterword b1;
		};
	};
} two_halves;

typedef struct {
	quarterword b0;
	quarterword b1;
	quarterword b2;
	quarterword b3;
} four_quarters;

typedef struct
{
	union {
		int an_int;
		two_halves hh;
		four_quarters qqqq;
	};
} memory_word;

typedef FILE* word_file;

// 158
#define pointer halfword // a flag or a location in mem or eqtb
#define null mem_min // the null pointer


// 159
Array<memory_word, mem_min, mem_max> mem; // the big dynamic storage area
pointer lo_mem_max; // the largest location of variable-size memory in use
pointer hi_mem_min; // the smallest location of one-word memory in use

// 160
int var_used;
int dyn_used; // how much memory is in use

// 161
#define link(n) mem[n].hh.rh // the link field of a memory word
#define info(n) mem[n].hh.lh // the info field of a memory word

pointer avail; // head of the list of available one-word nodes
pointer mem_end; // the last one-word node used in mem

// 165

// fast_get_avail, NOTE: use get_avail instead
#define fast_get_avail(n) n = get_avail()


// 166
#define empty_flag max_halfword // the link of an empty variable-size node
#define is_empty(n) (link(n) == empty_flag) // tests for empty node
#define node_size info // the size field in empty variable-size nodes
#define llink(n) (info(n + 1)) // left link in doubly-linked list of empty nodes
#define rlink(n) (link(n + 1)) // right link in doubly-linked list of empty nodes

pointer rover; // points to some node in the list of empties

// 175
const int null_coords = mem_min; // specification for pen offsets of (0,0)
const int null_pen = null_coords + 3; // we will define coord_node_size = 3
const int dep_head = null_pen + 10; // and pen_node_size = 10
const int zero_val = dep_head + 2; // two words for a permanently zero value
const int temp_val = zero_val + 2; // two words for a temporary value node
const int end_attr = temp_val; // we use end_attr + 2 only
const int inf_val = end_attr + 2; // and inf_val + 1 only
const int bad_vardef = inf_val + 2; // two words for vardef error recovery
const int lo_mem_stat_max = bad_vardef + 1; // largest allocated word in the variable-size mem
const int sentinel = mem_top; // end of sorted lists
const int temp_head = mem_top - 1; // head of a temporary list of some kind
const int hold_head = mem_top - 2; // head of a temporary list of another kind
const int hi_mem_stat_min = mem_top - 2; // smallest statically allocated word in the one-word mem


// 178
//debug
#ifndef NO_DEBUG
Array<bool, 0, mem_max> _free; // free cells
Array<bool, 0, mem_max> was_free; // previously free cells
pointer was_mem_end;
pointer was_lo_max;
pointer was_hi_min; // previous mem_end, lo_mem_max, and hi_mem_min
bool panicking; // do we want to check memory constantly?
#endif
//gubed


// 186
enum command_codes
{
	if_test=1, //{conditional text (\&{if})}
	fi_or_else=2, //{delimiters for conditionals (\&{elseif}, \&{else}, \&{fi})}
	input=3, //{input a source file (\&{input}, \&{endinput})}
	iteration=4, //{iterate (\&{for}, \&{forsuffixes}, \&{forever}, \&{endfor})}
	repeat_loop=5, //{special command substituted for \&{endfor}}
	exit_test=6, //{premature exit from a loop (\&{exitif})}
	relax=7, //{do nothing (\.{\char`\\})}
	scan_tokens=8, //{put a string into the input buffer}
	expand_after=9, //{look ahead one token}
	defined_macro=10, //{a macro defined by the user}
	min_command=defined_macro+1,
	display_command=11, //{online graphic output (\&{display})}
	save_command=12, //{save a list of tokens (\&{save})}
	interim_command=13, //{save an internal quantity (\&{interim})}
	let_command=14, //{redefine a symbolic token (\&{let})}
	new_internal=15, //{define a new internal quantity (\&{newinternal})}
	macro_def=16, //{define a macro (\&{def}, \&{vardef}, etc.)}
	ship_out_command=17, //{output a character (\&{shipout})}
	add_to_command=18, //{add to edges (\&{addto})}
	cull_command=19, //{cull and normalize edges (\&{cull})}
	tfm_command=20, //{command for font metric info (\&{ligtable}, etc.)}
	protection_command=21, //{set protection flag (\&{outer}, \&{inner})}
	show_command=22, //{diagnostic output (\&{show}, \&{showvariable}, etc.)}
	mode_command=23, //{set interaction level (\&{batchmode}, etc.)}
	random_seed=24, //{initialize random number generator (\&{randomseed})}
	message_command=25, //{communicate to user (\&{message}, \&{errmessage})}
	every_job_command=26, //{designate a starting token (\&{everyjob})}
	delimiters=27, //{define a pair of delimiters (\&{delimiters})}
	open_window=28, //{define a window on the screen (\&{openwindow})}
	special_command=29, //{output special info (\&{special}, \&{numspecial})}
	type_name=30, //{declare a type (\&{numeric}, \&{pair}, etc.)}
	max_statement_command=type_name,
	min_primary_command=type_name,
	left_delimiter=31, //{the left delimiter of a matching pair}
	begin_group=32, //{beginning of a group (\&{begingroup})}
	nullary=33, //{an operator without arguments (e.g., \&{normaldeviate})}
	unary=34, //{an operator with one argument (e.g., \&{sqrt})}
	str_op=35, //{convert a suffix to a string (\&{str})}
	cycle=36, //{close a cyclic path (\&{cycle})}
	primary_binary=37, //{binary operation taking `\&{of}' (e.g., \&{point})}
	capsule_token=38, //{a value that has been put into a token list}
	string_token=39, //{a string constant (e.g., |"hello"|)}
	internal_quantity=40, //{internal numeric parameter (e.g., \&{pausing})}
	min_suffix_token=internal_quantity,
	tag_token=41, //{a symbolic token without a primitive meaning}
	numeric_token=42, //{a numeric constant (e.g., \.{3.14159})}
	max_suffix_token=numeric_token,
	plus_or_minus=43, //{either `\.+' or `\.-'}
	max_primary_command=plus_or_minus, //{should also be |numeric_token+1|}
	min_tertiary_command=plus_or_minus,
	tertiary_secondary_macro=44, //{a macro defined by \&{secondarydef}}
	tertiary_binary=45, //{an operator at the tertiary level (e.g., `\.{++}')}
	max_tertiary_command=tertiary_binary,
	left_brace=46, //{the operator `\.{\char`\{}'}
	min_expression_command=left_brace,
	path_join=47, //{the operator `\.{..}'}
	ampersand=48, //{the operator `\.\&'}
	expression_tertiary_macro=49, //{a macro defined by \&{tertiarydef}}
	expression_binary=50, //{an operator at the expression level (e.g., `\.<')}
	equals=51, //{the operator `\.='}
	max_expression_command=equals,
	and_command=52, //{the operator `\&{and}'}
	min_secondary_command=and_command,
	secondary_primary_macro=53, //{a macro defined by \&{primarydef}}
	slash=54, //{the operator `\./'}
	secondary_binary=55, //{an operator at the binary level (e.g., \&{shifted})}
	max_secondary_command=secondary_binary,
	param_type=56, //{type of parameter (\&{primary}, \&{expr}, \&{suffix}, etc.)}
	controls=57, //{specify control points explicitly (\&{controls})}
	tension=58, //{specify tension between knots (\&{tension})}
	at_least=59, //{bounded tension value (\&{atleast})}
	curl_command=60, //{specify curl at an end knot (\&{curl})}
	macro_special=61, //{special macro operators (\&{quote}, \.{\#\AT!}, etc.)}
	right_delimiter=62, //{the right delimiter of a matching pair}
	left_bracket=63, //{the operator `\.['}
	right_bracket=64, //{the operator `\.]'}
	right_brace=65, //{the operator `\.{\char`\}}'}
	with_option=66, //{option for filling (\&{withpen}, \&{withweight})}
	cull_op=67, //{the operator `\&{keeping}' or `\&{dropping}'}
	thing_to_add=68, //{variant of \&{addto} (\&{contour}, \&{doublepath}, \&{also})}
	of_token=69, //{the operator `\&{of}'}
	from_token=70, //{the operator `\&{from}'}
	to_token=71, //{the operator `\&{to}'}
	at_token=72, //{the operator `\&{at}'}
	in_window=73, //{the operator `\&{inwindow}'}
	step_token=74, //{the operator `\&{step}'}
	until_token=75, //{the operator `\&{until}'}
	lig_kern_token=76, //{the operators `\&{kern}' and `\.{=:}' and `\.{=:\char'174}', etc.}
	assignment=77, //{the operator `\.{:=}'}
	skip_to=78, //{the operation `\&{skipto}'}
	bchar_label=79, //{the operator `\.{\char'174\char'174:}'}
	double_colon=80, //{the operator `\.{::}'}
	colon=81, //{the operator `\.:'}
	comma=82, //{the operator `\.,', must be |colon+1|}
	#define end_of_statement cur_cmd > comma
	semicolon=83, //{the operator `\.;', must be |comma+1|}
	end_group=84, //{end a group (\&{endgroup}), must be |semicolon+1|}
	stop=85, //{end a job (\&{end}, \&{dump}), must be |end_group+1|}
	max_command_code=stop,
	outer_tag=max_command_code+1, //{protection code added to command code}
};

typedef int command_code; // 1..max_command_code


// 187
enum meta_types
{
	undefined=0, //{no type has been declared}
	unknown_tag=1, //{this constant is added to certain type codes below}
	vacuous=1, //{no expression was present}
	boolean_type=2, //{\&{boolean} with a known value}
	unknown_boolean=boolean_type+unknown_tag,
	string_type=4, //{\&{string} with a known value}
	unknown_string=string_type+unknown_tag,
	pen_type=6, //{\&{pen} with a known value}
	unknown_pen=pen_type+unknown_tag,
	future_pen=8, //{subexpression that will become a \&{pen} at a higher level}
	path_type=9, //{\&{path} with a known value}
	unknown_path=path_type+unknown_tag,
	picture_type=11, //{\&{picture} with a known value}
	unknown_picture=picture_type+unknown_tag,
	transform_type=13, //{\&{transform} variable or capsule}
	pair_type=14, //{\&{pair} variable or capsule}
	numeric_type=15, //{variable that has been declared \&{numeric} but not used}
	known=16, //{\&{numeric} with a known value}
	dependent=17, //{a linear combination with |fraction| coefficients}
	proto_dependent=18, //{a linear combination with |scaled| coefficients}
	independent=19, //{\&{numeric} with unknown value}
	token_list=20, //{variable name or suffix argument or text argument}
	structured=21, //{variable with subscripts and attributes}
	unsuffixed_macro=22, //{variable defined with \&{vardef} but no \.{\AT!\#}}
	suffixed_macro=23, //{variable defined with \&{vardef} and \.{\AT!\#}}
};

#define unknown_types unknown_boolean: case unknown_string: case unknown_pen: case unknown_picture: case unknown_path

// 188
enum enum_name_type
{
	root=0, //{|name_type| at the top level of a variable}
	saved_root=1, //{same, when the variable has been saved}
	structured_root=2, //{|name_type| where a |structured| branch occurs}
	subscr=3, //{|name_type| in a subscript node}
	attr=4, //{|name_type| in an attribute node}
	x_part_sector=5, //{|name_type| in the \&{xpart} of a node}
	y_part_sector=6, //{|name_type| in the \&{ypart} of a node}
	xx_part_sector=7, //{|name_type| in the \&{xxpart} of a node}
	xy_part_sector=8, //{|name_type| in the \&{xypart} of a node}
	yx_part_sector=9, //{|name_type| in the \&{yxpart} of a node}
	yy_part_sector=10, //{|name_type| in the \&{yypart} of a node}
	capsule=11, //{|name_type| in stashed-away subexpressions}
	token=12, //{|name_type| in a numeric token or string token}
};

// 189
enum enum_operation_code
{
	true_code=30, //{operation code for \.{true}}
	false_code=31, //{operation code for \.{false}}
	null_picture_code=32, //{operation code for \.{nullpicture}}
	null_pen_code=33, //{operation code for \.{nullpen}}
	job_name_op=34, //{operation code for \.{jobname}}
	read_string_op=35, //{operation code for \.{readstring}}
	pen_circle=36, //{operation code for \.{pencircle}}
	normal_deviate=37, //{operation code for \.{normaldeviate}}
	odd_op=38, //{operation code for \.{odd}}
	known_op=39, //{operation code for \.{known}}
	unknown_op=40, //{operation code for \.{unknown}}
	not_op=41, //{operation code for \.{not}}
	decimal=42, //{operation code for \.{decimal}}
	reverse=43, //{operation code for \.{reverse}}
	make_path_op=44, //{operation code for \.{makepath}}
	make_pen_op=45, //{operation code for \.{makepen}}
	total_weight_op=46, //{operation code for \.{totalweight}}
	oct_op=47, //{operation code for \.{oct}}
	hex_op=48, //{operation code for \.{hex}}
	ASCII_op=49, //{operation code for \.{ASCII}}
	char_op=50, //{operation code for \.{char}}
	length_op=51, //{operation code for \.{length}}
	turning_op=52, //{operation code for \.{turningnumber}}
	x_part=53, //{operation code for \.{xpart}}
	y_part=54, //{operation code for \.{ypart}}
	xx_part=55, //{operation code for \.{xxpart}}
	xy_part=56, //{operation code for \.{xypart}}
	yx_part=57, //{operation code for \.{yxpart}}
	yy_part=58, //{operation code for \.{yypart}}
	sqrt_op=59, //{operation code for \.{sqrt}}
	m_exp_op=60, //{operation code for \.{mexp}}
	m_log_op=61, //{operation code for \.{mlog}}
	sin_d_op=62, //{operation code for \.{sind}}
	cos_d_op=63, //{operation code for \.{cosd}}
	floor_op=64, //{operation code for \.{floor}}
	uniform_deviate=65, //{operation code for \.{uniformdeviate}}
	char_exists_op=66, //{operation code for \.{charexists}}
	angle_op=67, //{operation code for \.{angle}}
	cycle_op=68, //{operation code for \.{cycle}}
	plus=69, //{operation code for \.+}
	minus=70, //{operation code for \.-}
	times=71, //{operation code for \.*}
	over=72, //{operation code for \./}
	pythag_add=73, //{operation code for \.{++}}
	pythag_sub=74, //{operation code for \.{+-+}}
	or_op=75, //{operation code for \.{or}}
	and_op=76, //{operation code for \.{and}}
	less_than=77, //{operation code for \.<}
	less_or_equal=78, //{operation code for \.{<=}}
	greater_than=79, //{operation code for \.>}
	greater_or_equal=80, //{operation code for \.{>=}}
	equal_to=81, //{operation code for \.=}
	unequal_to=82, //{operation code for \.{<>}}
	concatenate=83, //{operation code for \.\&}
	rotated_by=84, //{operation code for \.{rotated}}
	slanted_by=85, //{operation code for \.{slanted}}
	scaled_by=86, //{operation code for \.{scaled}}
	shifted_by=87, //{operation code for \.{shifted}}
	transformed_by=88, //{operation code for \.{transformed}}
	x_scaled=89, //{operation code for \.{xscaled}}
	y_scaled=90, //{operation code for \.{yscaled}}
	z_scaled=91, //{operation code for \.{zscaled}}
	intersect=92, //{operation code for \.{intersectiontimes}}
	double_dot=93, //{operation code for improper \.{..}}
	substring_of=94, //{operation code for \.{substring}}
	min_of=substring_of,
	subpath_of=95, //{operation code for \.{subpath}}
	direction_time_of=96, //{operation code for \.{directiontime}}
	point_of=97, //{operation code for \.{point}}
	precontrol_of=98, //{operation code for \.{precontrol}}
	postcontrol_of=99, //{operation code for \.{postcontrol}}
	pen_offset_of=100, //{operation code for \.{penoffset}}
};


// 190
enum internal_params
{
	tracing_titles=1, //{show titles online when they appear}
	tracing_equations=2, //{show each variable when it becomes known}
	tracing_capsules=3, //{show capsules too}
	tracing_choices=4, //{show the control points chosen for paths}
	tracing_specs=5, //{show subdivision of paths into octants before digitizing}
	tracing_pens=6, //{show details of pens that are made}
	tracing_commands=7, //{show commands and operations before they are performed}
	tracing_restores=8, //{show when a variable or internal is restored}
	tracing_macros=9, //{show macros before they are expanded}
	tracing_edges=10, //{show digitized edges as they are computed}
	tracing_output=11, //{show digitized edges as they are output}
	tracing_stats=12, //{show memory usage at end of job}
	tracing_online=13, //{show long diagnostics on terminal and in the log file}
	year=14, //{the current year (e.g., 1984)}
	month=15, //{the current month (e.g, 3 $\equiv$ March)}
	day=16, //{the current day of the month}
	_time=17, //{the number of minutes past midnight when this job started}
	char_code=18, //{the number of the next character to be output}
	char_ext=19, //{the extension code of the next character to be output}
	char_wd=20, //{the width of the next character to be output}
	char_ht=21, //{the height of the next character to be output}
	char_dp=22, //{the depth of the next character to be output}
	char_ic=23, //{the italic correction of the next character to be output}
	char_dx=24, //{the device's $x$ movement for the next character, in pixels}
	char_dy=25, //{the device's $y$ movement for the next character, in pixels}
	design_size=26, //{the unit of measure used for |char_wd..char_ic|, in points}
	hppp=27, //{the number of horizontal pixels per point}
	vppp=28, //{the number of vertical pixels per point}
	x_offset=29, //{horizontal displacement of shipped-out characters}
	y_offset=30, //{vertical displacement of shipped-out characters}
	pausing=31, //{positive to display lines on the terminal before they are read}
	showstopping=32, //{positive to stop after each \&{show} command}
	fontmaking=33, //{positive if font metric output is to be produced}
	proofing=34, //{positive for proof mode, negative to suppress output}
	smoothing=35, //{positive if moves are to be ``smoothed''}
	autorounding=36, //{controls path modification to ``good'' points}
	granularity=37, //{autorounding uses this pixel size}
	fillin=38, //{extra darkness of diagonal lines}
	turning_check=39, //{controls reorientation of clockwise paths}
	warning_check=40, //{controls error message when variable value is large}
	boundary_char=41, //{the right boundary character for ligatures}
	max_given_internal=41,
};



Array<scaled, 1, max_internal> internal; // the values of internal quantities
Array<str_number, 1, max_internal> int_name; // their names
int int_ptr; // max_given_internal..max_internal, the maximum internal quantity defined so far


// 196
int old_setting; // 0..max_selector

// 198
enum enum_char_class {
	digit_class = 0, // the class numbers 0123456789
	period_class = 1, // the class number '.'
	space_class = 2, // the class number of spaces and nonstandard characters
	percent_class = 3, // the class number of '%'
	string_class = 4, // the class number of '"'
	right_paren_class = 8, // the class number of ')'	
	letter_class = 9, // letters and the underline character
	left_bracket_class = 17, // '['
	right_bracket_class = 18, // ']'
	invalid_class = 20, // bad character in the input
	max_class = 20, // the largest class number
};

#define isolated_classes 5:case 6:case 7:case 8 // characters that make length-one tokens only




Array<unsigned char, 0, 255> char_class; // 0..max_class, the class numbers


// 200
#define next(n) (hash[n].lh) // link for coalesced lists
#define text(n) (hash[n].rh) // string number for symbolic token name
#define eq_type(n) (eqtb[n].lh) // the current "meaning" of a symbolic token
#define equiv(n) (eqtb[n].rh) // parametric part of a token's meaning
const int hash_base = 257; // hashing actually starts here
#define hash_is_full (hash_used == hash_base) // are all positions occupied?

pointer hash_used; // allocation pointer for hash
int st_count; // total number of known identifiers


// 201
const int hash_top = hash_base + hash_size; // the first location of the frozen area
const int frozen_inaccessible = hash_top; // hash location to protect the frozen area
const int frozen_repeat_loop = hash_top + 1; // hash location of a loop-repeat token
const int frozen_right_delimiter = hash_top + 2; // hash location of a permanent ')'
const int frozen_left_bracket = hash_top + 3; // hash location of a permanent '['
const int frozen_slash = hash_top + 4; // hash location of a permanent '/'
const int frozen_colon = hash_top + 5; // hash location of a permanent ':'
const int frozen_semicolon = hash_top + 6; // hash location of a permanent ';'
const int frozen_end_for = hash_top + 7; // hash location of a permanent endfor
const int frozen_end_def = hash_top + 8; // hash location of a permanent enddef
const int frozen_fi = hash_top + 9; // hash location of a permanent fi
const int frozen_end_group = hash_top + 10; // hash location of a permanent 'endgroup'
const int frozen_bad_vardef = hash_top + 11; // hash location of 'a bad variable'
const int frozen_undefined = hash_top + 12; // hash location that never gets defined
const int hash_end = hash_top + 12; // the actual size of the hash and eqtb arrays


Array<two_halves, 1, hash_end> hash; // the hash table
Array<two_halves, 1, hash_end> eqtb; // the equivalents

// 214
#define type(n) mem[n].hh.b0 // identifies what kind of node this is
#define name_type(n) mem[n].hh.b1 // a clue to the name of this value
const int token_node_size = 2; // the number of words in a large token node
#define value_loc(n) (n + 1) // the word that contains the value field
#define value(n) mem[value_loc(n)].an_int
#define expr_base (hash_end + 1) // code for the zeroth expr parameter
#define suffix_base (expr_base + param_size) // code for the zeroth suffix parameter
#define text_base (suffix_base + param_size) // code for the zeroth text parameter


// 225
pointer g_pointer; // (global) parameter to the foreword procedures

// 226
#define ref_count info // reference count preceding a macro definition or pen header
#define add_mac_ref(n) ref_count(n)++ // make a new reference to a macro list
const int general_macro = 0; // preface to a macro defined with a parameter list
const int primary_macro = 1; // preface to a macro with a primary parameter
const int secondary_macro = 2; // preface to a macro with a secondary parameter
const int tertiary_macro = 3; // preface to a macro with a tertiary parameter
const int expr_macro = 4; // preface to a macro with an undelimted expr parameter
const int of_macro = 5; // preface to a macro with undelimited 'expr x of y' parameters
const int suffix_macro = 6; // preface to a macro with an undelimited suffix parameter
const int text_macro = 7; // preface to a macro with an undelimited text parameter


// 228
#define subscr_head_loc(n) (n+1) // where value, subscr_head, and attr_head are
#define attr_head(n) info(subscr_head_loc(n)) // pointer to attribute info
#define subscr_head(n) link(subscr_head_loc(n)) // pointer to subscript info
const int value_node_size = 2; // the number of words in a value node

// 229
#define attr_loc_loc(n)  (n + 2) // where the attr_loc and parent fields are
#define attr_loc(n) info(attr_loc_loc(n)) // hash address of this attribute
#define parent(n) link(attr_loc_loc(n)) // pointer to structured variable
#define subscript_loc(n) (n + 2) // where the subscript field lives
#define subscript(n) mem[subscript_loc(n)].sc // subscript of this variable
const int attr_node_size = 3; // the number of words in an attribute node
const int subscr_node_size = 3; // the number of words in a subscript node
const int collective_subscript = 0; // code for the attribute '[]'



// 230
#define x_part_loc(n) n // where the xpart is found in a pair or transform node
#define y_part_loc(n) n+2
#define xx_part_loc(n) n+4
#define xy_part_loc(n) n+6
#define yx_part_loc(n) n+8
#define yy_part_loc(n) n+10
const int pair_node_size = 4;
const int transform_node_size = 12;

Array<small_number, transform_type, pair_type> big_node_size;

// 242
#define abort_find do {\
	return null;\
} while(false)



// 250
const int save_node_size = 2;
#define saved_equiv(n) mem[n+1].hh
#define save_boundary_item(n) do {\
	n = get_avail(); info(n) = 0; link(n) = save_ptr; save_ptr = n;\
} while (false)


pointer save_ptr; // the most recently saved item

// 255
#define left_type(n) mem[n].hh.b0 // characterizes the path entering this knot
#define right_type(n) mem[n].hh.b1 // characterizes the path leaving this knot
const int endpoint = 0; // left_type at path beginning and right_type at path end
#define x_coord(n) mem[n + 1].sc // the x coordinate of this knot
#define y_coord(n) mem[n + 2].sc // the y coordinate of this knot
#define left_x(n) mem[n + 3].sc // the x coordinate of previous control point
#define left_y(n) mem[n + 4].sc // the y coordinate of previous control point
#define right_x(n) mem[n + 5].sc // the x coordinate of next control point
#define right_y(n) mem[n + 6].sc // the y coordinate of next control point
const int knot_node_size = 7; // number of words in a knot node


// 256
#define left_curl left_x //{curl information when entering this knot}
#define left_given left_x //{given direction when entering this knot}
#define left_tension left_y //{tension information when entering this knot}
#define right_curl right_x //{curl information when leaving this knot}
#define right_given right_x //{given direction when leaving this knot}
#define right_tension right_y //{tension information when leaving this knot}
const int _explicit=1; //{|left_type| or |right_type| when control points are known}
const int given=2; //{|left_type| or |right_type| when a direction is given}
const int curl=3; //{|left_type| or |right_type| when a curl is desired}
const int _open_=4; //{|left_type| or |right_type| when \MF\ should choose the direction}




// 267
pointer path_tail; // the node that links to the beginning of a path

// 272
const int end_cycle = _open_ + 1;


// 279
Array<scaled, 0, path_size> delta_x; // knot differences
Array<scaled, 0, path_size> delta_y;
Array<scaled, 0, path_size> delta;
Array<angle, 1, path_size> psi; // turning angles

// 283
Array<angle, 0, path_size> theta; // values of \theta_k
Array<fraction, 0, path_size> uu; // values of \u_k
Array<angle, 0, path_size> vv; // values of v_k
Array<fraction, 0, path_size> ww; // values of w_k

// 292
#define reduce_angle(n) do {\
	if (myabs(n) > one_eighty_deg)\
		if (n > 0) n = n - three_sixty_deg; else n = n + three_sixty_deg;\
} while(false)

// 298
fraction st; // sines and cosines
fraction ct;
fraction sf;
fraction cf;


Array<int, 0, move_size> move; // the recorded moves
int move_ptr; // 0..move_size, the number of items in the move list


// 309
#define stack_x1 bisect_stack[bisect_ptr] //{stacked value of $X_1$}
#define stack_x2 bisect_stack[bisect_ptr+1] //{stacked value of $X_2$}
#define stack_x3 bisect_stack[bisect_ptr+2] //{stacked value of $X_3$}
#define stack_r bisect_stack[bisect_ptr+3] //{stacked value of $R$}
#define stack_m bisect_stack[bisect_ptr+4] //{stacked value of $m$}
#define stack_y1 bisect_stack[bisect_ptr+5] //{stacked value of $Y_1$}
#define stack_y2 bisect_stack[bisect_ptr+6] //{stacked value of $Y_2$}
#define stack_y3 bisect_stack[bisect_ptr+7] //{stacked value of $Y_3$}
#define stack_s bisect_stack[bisect_ptr+8] //{stacked value of $S$}
#define stack_n bisect_stack[bisect_ptr+9] //{stacked value of $n$}
#define stack_l bisect_stack[bisect_ptr+10] //{stacked value of $l$}
const int move_increment=11; //{number of items pushed by |make_moves|}
Array<int, 0, bistack_size> bisect_stack;
int bisect_ptr; // 0..bistack_size


// 324
const int zero_w = 4;
const int _void = null + 1;

// 325
#define knil info // inverse of the link field, in a doubly linked list
#define sorted_loc(n) (n + 1) // where the sorted link field resides
#define sorted(n) link(sorted_loc(n)) // beginning of the list of sorted edge weights
#define unsorted(n) info(n + 1) // beginning of the list of unsorted edge weights
const int row_node_size = 2; // number of words in a row header node


// 326
const int zero_field = 4096; // amount added to coordiantes to make them positive
#define n_min(n) info(n + 1) // minimum row number present, plus zero_field
#define n_max(n) link(n + 1) // maximum row number present, plus zero_field
#define m_min(n) info(n + 2) // minimum column number present, pluz zero_field
#define m_max(n) link(n + 2) // maximum column number present, pluz zero_field
#define m_offset(n) info(n + 3) // translation of m data in edge-weight nodes
#define last_window(n) link(n + 3) // the last display went into this window
#define last_window_time(n) mem[n + 4].an_int // after this many window updates
#define n_pos(n) info(n + 5) // the row currenlty in n_rover, plus zero_field
#define n_rover(n) link(n + 5) // a row recently referenced
const int edge_header_size = 6; // number of words in an edge-structure header
#define valid_range(n) (myabs(n - 4096) < 4096) // is n strictly between 0 and 8192?
#define empty_edges(n) link(n) == n // are there no rows in this edge header?



// 327
pointer cur_edges; // the edge structure of current interest
int cur_wt; // the edge weight of current interest

// 371
int trace_x; // x coordinate most recently shown in a trace
int trace_y; // y coordinate most recently shown in a trace
int trace_yy; // y coordinate most recently encountered


// 379
int octant; // first_octant..sixth_octant, the current octant of interest

// 387
#define set_two(x, y) do {\
	cur_x = x; cur_y = y;\
} while(false)


// 389
scaled cur_x; // outputs of skew, unskew, and a few others
scaled cur_y;

// 391
#define no_crossing return fraction_one + 1
#define one_crossing return fraction_one
#define zero_crossing return 0



// 393
#define right_octant right_x // the octant code before a transition
#define left_octant left_x // the octant after a transition
#define right_transition right_y // the type of transition
#define left_transition left_y // ditto, either axis or diagonal
const int axis = 0; // a transition across the x'- or y'-axis
const int diagonal = 1; // a transition where y' = +-x'

// 394
#define print_two_true(a,b) do {\
	unskew(a, b, octant); print_two(cur_x, cur_y);\
} while(false)


// 395
Array<str_number, first_octant, sixth_octant> octant_dir;

// 403
const int double_path_code = 0; // command modifier for 'doublepath'
const int contour_code = 1; // commaond modifier for 'contour'
const int also_code = 2; // command modifier for 'also'

pointer cur_spec; // the principal output of make_spec
int turning_number; // another output of make_spec
pointer cur_pen; // an implicit input of make_spec, used in autorounding
int cur_path_type; // double_path_code..contour_code, likewise
scaled max_allowed; // coordinates must be at most this big


// 404
#define procrustes(n) do {\
	if (myabs(n) >= dmax)\
		if (myabs(n) > max_allowed) {\
			chopped = 1;\
			if (n > 0) n = max_allowed; else n = -max_allowed;\
		}\
		else if (chopped == 0) chopped = -1;\
} while(false)

// 410
#define t_of_the_way(a, b) ((a) - take_fraction((a) - (b), t))



// 427
Array<scaled, 0, max_wiggle> before; // data for make_safe
Array<scaled, 0, max_wiggle> after;
Array<pointer, 0, max_wiggle> node_to_round; // reference back to the path
int cur_rounding_ptr; // 0..max_wiggle, how many are being used
int max_rounding_ptr; // 0..max_wiggle, how many have been used

// 430
scaled cur_gran; // the current granularity (which normally is unity)

// 435
#define north_edge(n) y_coord(link(n + fourth_octant))
#define south_edge(n) y_coord(link(n + first_octant))
#define east_edge(n) y_coord(link(n + second_octant))
#define west_edge(n) y_coord(link(n + seventh_octant))


// 442
#define diag_offset(n) x_coord(knil(link(cur_pen + n)))

// 448
Array<int, first_octant, sixth_octant> octant_number; // 1..8
Array<int, 1, 8> octant_code; // first_octant..sixth_octant


// 455
bool rev_turns; // should we make U-turns in the English manner?


// 461
Array<int, first_octant, sixth_octant> y_corr; // 0..1
Array<int, first_octant, sixth_octant> xy_corr; // 0..1
Array<int, first_octant, sixth_octant> z_corr; // 0..1
Array<int, first_octant, sixth_octant> x_corr; // -1..1


// 464
int m0, n0, m1, n1; // lattice point coordinates
int d0, d1; // 0..1, displacement corrections

// 472
const int pen_node_size = 10;
const int coord_node_size = 3;
#define max_offset(n) mem[n+9].sc



// 487
#define add_pen_ref(n) ref_count(n)++
#define delete_pen_ref(n) do {\
	if(ref_count(n) == null) toss_pen(n); else ref_count(n)--;\
} while(false)

// 507
Array<int, 0, move_size> env_move;

// 528
#define right_u right_x // u value for a pen edge
#define left_v left_x // v value for a pen edge
#define right_class right_y // equivalence class number of a pen edge
#define left_length left_y // length of a pen edge


// 547
#define we_found_it do {\
	tt = (t + 04000) /  010000; goto found;\
} while(false)

// 552
int tol_step; // 0..6, either 0 or 3, usually

// 553
#define stack_1(n) bisect_stack[n] //{$U_1$, $V_1$, $X_1$, or $Y_1$}
#define stack_2(n) bisect_stack[n+1] //{$U_2$, $V_2$, $X_2$, or $Y_2$}
#define stack_3(n) bisect_stack[n+2] //{$U_3$, $V_3$, $X_3$, or $Y_3$}
#define stack_min(n) bisect_stack[n+3] //{$U\submin$, $V\submin$, $X\submin$, or $Y\submin$}
#define stack_max(n) bisect_stack[n+4] //{$U\submax$, $V\submax$, $X\submax$, or $Y\submax$}
const int int_packets=20; //{number of words to represent $U_k$, $V_k$, $X_k$, and $Y_k$}

#define u_packet(n) n-5
#define v_packet(n) n-10
#define x_packet(n) n-15
#define y_packet(n) n-20
#define l_packets bisect_ptr-int_packets
#define r_packets bisect_ptr
#define ul_packet u_packet(l_packets) //{base of $U'_k$ variables}
#define vl_packet v_packet(l_packets) //{base of $V'_k$ variables}
#define xl_packet x_packet(l_packets) //{base of $X'_k$ variables}
#define yl_packet y_packet(l_packets) //{base of $Y'_k$ variables}
#define ur_packet u_packet(r_packets) //{base of $U''_k$ variables}
#define vr_packet v_packet(r_packets) //{base of $V''_k$ variables}
#define xr_packet x_packet(r_packets) //{base of $X''_k$ variables}
#define yr_packet y_packet(r_packets) //{base of $Y''_k$ variables}

#define u1l stack_1(ul_packet) //{$U'_1$}
#define u2l stack_2(ul_packet) //{$U'_2$}
#define u3l stack_3(ul_packet) //{$U'_3$}
#define v1l stack_1(vl_packet) //{$V'_1$}
#define v2l stack_2(vl_packet) //{$V'_2$}
#define v3l stack_3(vl_packet) //{$V'_3$}
#define x1l stack_1(xl_packet) //{$X'_1$}
#define x2l stack_2(xl_packet) //{$X'_2$}
#define x3l stack_3(xl_packet) //{$X'_3$}
#define y1l stack_1(yl_packet) //{$Y'_1$}
#define y2l stack_2(yl_packet) //{$Y'_2$}
#define y3l stack_3(yl_packet) //{$Y'_3$}
#define u1r stack_1(ur_packet) //{$U''_1$}
#define u2r stack_2(ur_packet) //{$U''_2$}
#define u3r stack_3(ur_packet) //{$U''_3$}
#define v1r stack_1(vr_packet) //{$V''_1$}
#define v2r stack_2(vr_packet) //{$V''_2$}
#define v3r stack_3(vr_packet) //{$V''_3$}
#define x1r stack_1(xr_packet) //{$X''_1$}
#define x2r stack_2(xr_packet) //{$X''_2$}
#define x3r stack_3(xr_packet) //{$X''_3$}
#define y1r stack_1(yr_packet) //{$Y''_1$}
#define y2r stack_2(yr_packet) //{$Y''_2$}
#define y3r stack_3(yr_packet) //{$Y''_3$}

#define stack_dx bisect_stack[bisect_ptr] //{stacked value of |delx|}
#define stack_dy bisect_stack[bisect_ptr+1] //{stacked value of |dely|}
#define stack_tol bisect_stack[bisect_ptr+2] //{stacked value of |tol|}
#define stack_uv bisect_stack[bisect_ptr+3] //{stacked value of |uv|}
#define stack_xy bisect_stack[bisect_ptr+4] //{stacked value of |xy|}
const int int_increment=int_packets+int_packets+5; //{number of stack words per level}

// 554
#define set_min_max(n) do {\
	if (stack_1(n) < 0)\
		if (stack_3(n) >= 0)\
		{\
			if (stack_2(n) < 0) stack_min(n) = stack_1(n) + stack_2(n);\
			else stack_min(n) = stack_1(n);\
			stack_max(n) = stack_1(n) + stack_2(n) + stack_3(n);\
			if (stack_max(n) < 0) stack_max(n) = 0;\
		}\
		else {\
			stack_min(n) = stack_1(n) + stack_2(n) + stack_3(n);\
			if (stack_min(n) > stack_1(n)) stack_min(n) = stack_1(n);\
			stack_max(n) = stack_1(n) + stack_2(n);\
			if (stack_max(n) < 0) stack_max(n) = 0;\
		}\
	else if (stack_3(n) <= 0)\
	{\
		if (stack_2(n) > 0) stack_max(n) = stack_1(n) + stack_2(n);\
		else stack_max(n) = stack_1(n);\
		stack_min(n) = stack_1(n) + stack_2(n) + stack_3(n);\
		if (stack_min(n) > 0) stack_min(n) = 0;\
	}\
	else {\
		stack_max(n) = stack_1(n) + stack_2(n) + stack_3(n);\
		if (stack_max(n) < stack_1(n)) stack_max(n) = stack_1(n);\
		stack_min(n) = stack_1(n) + stack_2(n);\
		if (stack_min(n) > 0) stack_min(n) = 0;\
	}\
} while(false);






// 555
const int max_patience = 5000;

int cur_t, cur_tt; // controls and results of cubic_intersection
int time_to_go; // this many backtracks before giving up
int max_t; // maximum of 2^(l+1) so far achieved

// 557
int delx, dely; // the components \Delta = 2^l(w_0 - z_0)
int tol; // bound on the uncertainty in teh overlap test
int uv, xy; // 0..bistack_size, pointers to teh current packets of interest
int three_l; // tol_step times the bisection level
int appr_t, appr_tt; // best approximates known to the answers

// 565
const int white = 0; // background pixels
const int black = 1; // visible pixels

typedef int screen_row; // 0..screen_depth, a row number on the screen
typedef int screen_col; // 0..screen_width, a column number on the screen
typedef Array<screen_col, 0, screen_width> trans_spec; // a transition spec, see below
typedef int pixel_color; // white..black, specifies one of the two pixel values


// 566
Array<Array<pixel_color, 0, screen_width>, 0, screen_depth> screen_pixel;


// 569
bool screen_started; // have the screen primitives been initialized?
bool screen_OK; // is it legitimate to call blank_rectangle, paint_row, and update_screen?

// 570
#define start_screen do {\
	if (!screen_started) {\
		screen_OK = init_screen(); screen_started = true;\
	}\
} while (false)

// 571
typedef int window_number; // 0..15

// 572
Array<bool, 0, 15> window_open; // has this window been opened?
Array<screen_col, 0, 15> left_col; // leftmost column position on screen
Array<screen_col, 0, 15> right_col; // rightmost column position, plus 1
Array<screen_col, 0, 15> top_row; // topmost row position on screen
Array<screen_col, 0, 15> bot_row; // bottommost row position, plus 1
Array<screen_col, 0, 15> m_window; // offset between user and screen columns
Array<screen_col, 0, 15> n_window; // offset between user and screen rows
Array<screen_col, 0, 15> window_time; // it has been updated this often


// 579
trans_spec row_transition; // an array of black/white transitions


// 585
const int s_scale = 64; // the serial numbers are multiplied by this factor
#define new_indep(n) do { /*{create a new independent variable}*/\
	if (serial_no > el_gordo - s_scale)\
		overflow(/*independent variables*/1073,serial_no / s_scale);\
	type(n) = independent; serial_no = serial_no + s_scale;\
	value(n) = serial_no;\
} while(false)




int serial_no; // the most recent serial number, times s_scale

// 587
#define dep_list(n) link(value_loc(n)) // half of the value in a dependent variable
#define prev_dep(n) info(value_loc(n)) // the other half; makes a doubly linked list
const int dep_node_size = 2; // the number of words per dependency node
			   
// 592
const int coef_bound = 04525252525; // fraction approximation to 7/3
const int independent_needing_fix = 0;


bool fix_needed; // does at least one independent variable need scaling?
bool watch_coefs; // should we scale coefficients that exceed coef_bound
pointer dep_final; // location of the constant term and final link

// 594
const int fraction_threshold = 2685; // a fraction coefficient less than this is zeroed
const int half_fraction_threshold = 1342; // half of fraction_threshold
const int scaled_threshold = 8; // a scaled coefficient less than this is zeroed
const int half_scaled_threshold = 4; // half of scaled_threshold


// 605
const int independent_being_fixed = 1; // this variable already appears in s


// 624
eight_bits cur_cmd; // current command set by get_next
int cur_mod; // operand of current command
halfword cur_sym; // hash address of current symbol


// 626
#define show_cur_cmd_mod show_cmd_mod(cur_cmd, cur_mod)


// 627
typedef struct
{
	quarterword index_field;
	halfword start_field;
	halfword loc_field;
	halfword limit_field;
	halfword name_field;
} in_state_record;


// 628
Array<in_state_record, 0, stack_size> input_stack;
int input_ptr; // 0..stack_size, first unused location of input_stack
int max_in_stack; // 0..stack_size, largest value of input_ptr when pusing
in_state_record cur_input; // the ``top'' input state

// 629
#define index cur_input.index_field
#define start cur_input.start_field
#define limit cur_input.limit_field
#define name cur_input.name_field

// 631
#define terminal_input (name == 0) // are we reading from the terminal?
#define cur_file input_file[index] // the current alpha_file variable


int in_open; // 0..max_in_open, the number of lines in the buffer, less one
int open_parens; // 0..max_in_open, the number of open text files
Array<alpha_file, 1, max_in_open> input_file;
int line; // current line number in the current source file
Array<int, 1, max_in_open> line_stack;


// 632
#define token_type index // type of current token list
#define token_state (index > max_in_open) // are we scanning a token list?
#define file_state (index <= max_in_open) // are we scanning a file line?
#define param_start limit // base of macro parameters in param_stack
const int forever_text = max_in_open + 1; // token_type code for loop texts
const int loop_text = max_in_open + 2; // token_type code for loop texts
const int parameter = max_in_open + 3; // token_type code for parameter texts
const int backed_up = max_in_open + 4; // token_type code for texts to be reread
const int inserted = max_in_open + 5; // token_type code for inserted texts
const int macro = max_in_open + 6; // token_type code for macro replacement texts

// 633
Array<pointer, 0, param_size> param_stack; // token list pointers for parameters
int param_ptr; // 0..param_size, first unused entry in param_stack
int max_param_stack; // largest value of param_ptr

// 634
int file_ptr; // 0..stack_size, shallowest level shown by show_context

// 642
#define begin_pseudoprint do {l = tally;tally=0;selector=pseudo;trick_count=1000000;}while(0)
#define set_trick_count do {first_count = tally;trick_count=tally+1+error_line-half_error_line;if(trick_count<error_line)trick_count=error_line;}while(0)

// 647
#define push_input do {\
	if (input_ptr > max_in_stack) {\
		max_in_stack = input_ptr;\
		if (input_ptr == stack_size) overflow(/*input stack size*/1074, stack_size);\
	}\
	input_stack[input_ptr] = cur_input;\
	input_ptr++;\
} while(false)

#define pop_input do{\
	input_ptr--; cur_input = input_stack[input_ptr];\
} while(false)


// 649
#define back_list(s) begin_token_list(s, backed_up) // backs up a simple token list

// 659
const int normal = 0; // scanner_status at "quiet times"
const int skipping = 1; // scanner_status when false conditional text is being skipped
const int flushing = 2; // scanner_status when junk after a statement is being ignored
const int absorbing = 3; // scanner_status when a text parameter is being scanned
const int var_defining = 4; // scanner_status when a vardef is being scanned
const int op_defining = 5; // scanner_status when a macro def is being scanned
const int loop_defining = 6; // scanner_status when a for loop is being scanned

int scanner_status; // normal..loop_defining, are we scanning at high speed?
int warning_info; // if so, what else do we need to know, in case an error occurrs?


// 680
bool force_eof; // should the next input be aborted early?

// 683
const int start_def = 1;
const int var_def = 2;
const int end_def = 0;
const int start_forever = 1;
const int end_for = 0;

// 688
const int quote = 0;
const int macro_prefix = 1;
const int macro_at = 2;
const int macro_suffix = 3;

// 699
int bg_loc, eg_loc; // 1..hash_end, hash addresses of 'begingroup' and 'endgroup'

// 738
const int if_node_size = 2; // number of words in stack entry for conditionals
#define if_line_field(n) mem[n+1].an_int
const int if_code = 1; // code for if being evaluated
const int fi_code = 2; // code for fi
const int else_code = 3; // code for else
const int else_if_code = 4; // code for elseif

pointer cond_ptr; // top of the condition stack
int if_limit; // normal..else_if_code, upper bound on fi_or_else codes
small_number cur_if; // type of conditional being worked on
int if_line; // line where the conditional began

// 752
#define loop_list_loc(n) n+1 // where the loop_list field resides
#define loop_type(n) info(loop_list_loc(n)) // the type of for loopo
#define loop_list(n) link(loop_list_loc(n)) // the remaining elements
const int loop_node_size = 2; // the number of words in a loop control node
const int progression_node_size = 4; // the number of words in a progression node
#define step_size(n) mem[n+2].sc // the step size in an arithmetic progression
#define final_value(n) mem[n+3].sc // the final value in an arithmetic progression


pointer loop_ptr; // top of the loop-control-node stack


// 767
str_number cur_name; // name of file just scanned
str_number cur_area; // file area just scanned, or ""
str_number cur_ext; // file extension just scanned, or ""

// 768
pool_pointer area_delimiter; // the most recent '>' or ':', if any
pool_pointer ext_delimiter; // the relevant '.', if any

//769
// Note: this area is not used
#define MF_area /*MFinputs:*/1075

// 774
#define append_to_name(s) do{\
	c = s;\
	k++;\
	if(k <= file_name_size)\
		name_of_file[k] = xchr[c];\
} while(0)


// 775
//const int base_default_length = 11; // length of MF_base_default string strlen("plain.base")+1, NOTE: used to be 18
const int base_area_length = 0; // length of its area part, NOTE: used to be 8
const int base_ext_length = 5; // length of its '.base' part
#define base_extension /*.base*/1076 // the extension as a WEB constant

//Array<char, 1, base_default_length> MF_base_default;
const char *MF_base_default = "plain.base";

// 782
str_number job_name; // principal file name
bool log_opened; // has the transcript file been opened?
str_number log_name; // full name of the log file


// 784
#define pack_cur_name pack_file_name(cur_name, cur_area, cur_ext)


// 785
str_number gf_ext; // default extension for the output file

// 791
#define set_output_file_name do{\
	if (job_name == 0) open_log_file();\
	pack_job_name(gf_ext);\
	while (!b_open_out(&gf_file))\
		prompt_file_name(/*file name for output*/1077,gf_ext);\
	output_file_name = b_make_name_string(gf_file);\
} while (false)


byte_file gf_file; // the generic font output goes here
str_number output_file_name; // full name of the output file


// 796
small_number cur_type; // the type of the expression just found
int cur_exp; // the value of the expression just found


// 807
#define exp_err(n) disp_err(null, n) // displays the current expression


// 813
Array<int, dependent, proto_dependent> max_c; // max coefficient magnitude
Array<pointer, dependent, proto_dependent> max_ptr; // where p occurs with max_c
Array<pointer, dependent, proto_dependent> max_link; // other occurrences of p


// 821
int var_flag; // 0..max_command_code, command that wants a variable

// 883
#define min_tension three_quarter_unit

// 906
#define three_sixty_units 23592960 // that's 360*unity
#define boolean_reset(n) if (n) cur_exp = true_code; else cur_exp = false_code;

// 918
#define type_range(a, b) do {\
	if (cur_type >= a && cur_type <= b) flush_cur_exp(true_code);\
	else flush_cur_exp(false_code); cur_type = boolean_type;\
} while (false)

#define type_test(a) do {\
	if (cur_type == a) flush_cur_exp(true_code);\
	else flush_cur_exp(false_code); cur_type = boolean_type;\
} while(false)


// 954
scaled txx, txy, tyx, tyy, tx, ty; // current transform coefficients

// 1037
enum show_code
{
	show_token_code=0, //{show the meaning of a single token}
	show_stats_code=1, //{show current memory and string usage}
	show_code=2, //{show a list of expressions}
	show_var_code=3, //{show a variable and its descendents}
	show_dependencies_code=4, //{show dependent variables in terms of independents}
};

// 1052
const int drop_code = 0;
const int keep_code = 1;

// 1077
halfword start_sym; // a symbolic token to insert at beginning of job

// 1079
const int message_code = 0;
const int err_message_code = 1;
const int err_help_code = 2;


// 1084
bool long_help_seen; // has the long errmessage help been used?


// 1087
byte_file tfm_file; // the font metric output goes here
str_number metric_file_name; // full name of the font metric file

// 1092
const int no_tag = 0; // vanilla character
const int lig_tag = 1; // character has a ligature/kerning program
const int list_tag = 2; // character has successor in a charlist
const int ext_tag = 3; // character is extensible


// 1093
const int stop_flag = 128 + min_quarterword; // valud indicating 'STOP' in a lig/kern program
const int kern_flag = 128 + min_quarterword; // op code for a kern step
#define skip_byte(n) lig_kern[n].b0
#define next_char(n) lig_kern[n].b1
#define op_byte(n) lig_kern[n].b2
#define rem_byte(n) lig_kern[n].b3

// 1094
#define ext_top(n) exten[n].b0 // top piece in a recipe
#define ext_mid(n) exten[n].b1 // mid piece in a recipe
#define ext_bot(n) exten[n].b2 // bot piece in a recipe
#define ext_rep(n) exten[n].b3 // rep piece in a recipe


// 1096
#define undefined_label lig_table_size // an undefined local label


eight_bits bc, ec; // smallest and largest character codes shipped out
Array<scaled, 0, 255> tfm_width; // charwd values
Array<scaled, 0, 255> tfm_height; // charht values
Array<scaled, 0, 255> tfm_depth; // chardp values
Array<scaled, 0, 255> tfm_ital_corr; // charic values
Array<bool, 0, 255> char_exists; // has this code been shipped out?
Array<int, 0, 255> char_tag; // no_tag..ext_tag, remainder category
Array<int, 0, 255> char_remainder; // 0..lig_table_size, the remainder byte
Array<int, 1, header_size> header_byte; // -1..255, bytes of the TFM header, or -1 if unset
Array<four_quarters, 0, lig_table_size> lig_kern; // the ligature/kern table
int nl; // 0..32767-256, the number of ligature/kern steps so far
Array<scaled, 0, max_kerns> kern; // distinct kerning amounts
int nk; // 0..max_kerns, the number fo distinct kerns so far
Array<four_quarters, 0, 255> exten; // extensible character recipes
int ne; // 0..256, the number fo extensible characters so far
Array<scaled, 1, max_font_dimen> param; // fontinfo parameters
int np; // 0.. max_font_dimen, the largest fontinfo parameter specified so far
int nw, nh, nd, ni; // 0..256, sizes of TFM subtables
Array<int, 0, 255> skip_table; // 0..lig_table_size, local label status
bool lk_started; // has there been a lig/kern step in this command yet?
int bchar; // right boundary character
int bch_label; // 0..lig_table_size, left boundary starting location
int ll, lll; // 0..lig_table_size, registers used for lig/kern processing
Array<int, 0, 256> label_loc; // -1..lig_table_size, lig/kern starting addresses
Array<eight_bits, 1, 256> label_char; // characters for label_loc
int label_ptr; // 0..256, highest position occupied in label_loc



// 1101
const int char_list_code = 0;
const int lig_table_code = 1;
const int extensible_code = 2;
const int header_byte_code = 3;
const int font_dimen_code = 4;



// 1110
#define cancel_skips(n) do{\
	ll = n;\
	do {\
		lll = qo(skip_byte(ll)); skip_byte(ll) = stop_flag; ll = ll - lll;\
	} while(!(lll == 0));\
} while(false)

#define skip_error(n) do{\
	print_err(/*Too far to skip*/1078);\
	help1(/*At most 127 lig/kern steps can separate skipto1 from 1::.*/1079); error();\
	cancel_skips(n);\
} while(false)

// 1113
#define missing_extensible_punctuation(s) do {\
	missing_err(s);\
	help1(/*I'm processing `extensible c: t,m,b,r'.*/1080); back_error();\
} while (false)





// 1117
#define clear_the_list link(temp_head) = inf_val


// 1119
scaled perturbation; // quantity related to TFM rounding
int excess; // the list is this much too long

// 1125
Array<pointer, 1, 4> dimen_head; // lists of TFM dimensions

// 1128
const int three_bytes = 0100000000; // 2^24


// 1130
scaled max_tfm_dimen; // bound on widths, heights, kerns, etc.
int tfm_changed; // the number of data entries that were out of bounds

// 1144
const int gf_id_byte = 131; // identifies the kind of GF files described here


// 1145
enum opcodes
{
	paint_0=0, //{beginning of the \\{paint} commands}
	paint1=64, //{move right a given number of columns, then
	boc=67, //{beginning of a character}
	boc1=68, //{short form of |boc|}
	eoc=69, //{end of a character}
	skip0=70, //{skip no blank rows}
	skip1=71, //{skip over blank rows}
	new_row_0=74, //{move down one row and then right}
	max_new_row=164, //{the largest \\{new\_row} command is |new_row_164|}
	xxx1=239, //{for \&{special} strings}
	xxx3=241, //{for long \&{special} strings}
	yyy=243, //{for \&{numspecial} numbers}
	char_loc=245, //{character locators in the postamble}
	pre=247, //{preamble}
	post=248, //{postamble beginning}
	post_post=249, //{postamble ending}
};


// 1149
int gf_min_m, gf_max_m, gf_min_n, gf_max_n; // bounding rectangle
int gf_prev_ptr; // where the present/next character started/starts
int total_chars; // the number of characters output so far
Array<int, 0, 255> char_ptr; // where individual characters started
Array<int, 0, 255> gf_dx, gf_dy; // device escapements

// 1151
typedef int gf_index; // 0..gf_buf_size, an index into the output buffer

// 1152

Array<eight_bits, 0, gf_buf_size> gf_buf; // buffer for GF output
int half_buf; // 0..gf_buf_size, half of gf_buf_size
int gf_limit; // 0..gf_buf_size, end of the current half buffer
int gf_ptr; // 0..gf_buf_size, the next available buffer address
int gf_offset; // gf_buf_size times the number of times the output buffer has been fully emptied

// 1155
#define gf_out(n) do{\
	gf_buf[gf_ptr] = n; gf_ptr++;\
	if (gf_ptr == gf_limit) gf_swap();\
} while(false)

// 1161
#define one_byte(n) n >= 0 && n < 256


// 1162
int boc_c, boc_p; // parameters of the next boc command


// 1163
#define check_gf if (output_file_name == 0) init_gf();


// 1183
str_number base_ident;

// 1187
#define too_small(s) do{\
	wake_up_terminal();\
	wterm_s("---! Must increase the ");\
	wterm_ln_s(s);\
	goto off_base;\
} while(0)

// 1188
word_file base_file; // for input or output of base information

// 1189
#define undump_size(a,b,c,d) do{\
	undump_int(&x);\
	if(x < a) goto off_base;\
	if(x > b) too_small(c);\
	else d = x;\
} while(0)

// 1189
#define undump(a,b,c) do{\
	undump_int(&x);\
	if(x < a||x > b) goto off_base;\
	else c = x;\
} while(0)

// 1192
#define dump_four_ASCII do {\
	w.b0 = qi(str_pool[k]);\
	w.b1 = qi(str_pool[k+1]);\
	w.b2 = qi(str_pool[k+2]);\
	w.b3 = qi(str_pool[k+3]);\
	dump_qqqq(w);\
} while(0)

word_file fmt_file;

// 1193
#define undump_four_ASCII do{\
	undump_qqqq(&w);\
	str_pool[k] = qo(w.b0);\
	str_pool[k+1] = qo(w.b1);\
	str_pool[k+2] = qo(w.b2);\
	str_pool[k+3] = qo(w.b3);\
} while(0)

// 1203
//int ready_already; // NOTE: not used



// function prototypes

int myabs(int x);
bool myodd(int c);
void copypath(char *s1, char *s2, int n);
void set_paths();
void pack_real_name_of_file(char **cpp);
bool test_access(int filepath);
bool a_open_in(FILE **f, int path_specifier);
bool a_open_out(FILE **f);
bool b_open_out(FILE **f);
bool w_open_out(FILE **f);
void a_close(FILE *f);
void b_close(FILE *f);
void w_close(FILE *f);
void wake_up_terminal();
void jump_out();
void do_end_of_MF();
void setup_char_arrays();
void do_final_end();
bool input_ln(FILE *fp, bool);
bool init_terminal(int argc, char **argv);
void flush_string(str_number s);
str_number make_string();
bool str_eq_buf(str_number s, int k);
int str_vs_str(str_number s, str_number t);
bool get_strings_started();
void print_ln();
void print_char(ASCII_code s);
void print(int s);
void slow_print(int s);
void print_nl(str_number s);
void print_the_digs(eight_bits k);
void print_int(int n);
void print_dd(int n);
void error();
void normalize_selector();
void succumb();
void overflow(str_number s, int n);
void fix_date_and_time();
void confusion(str_number s);
void pause_for_instructions();
void clear_arith();
void print_scaled(scaled s);
void print_two(scaled x, scaled y);
scaled make_scaled(int p, int q);
pointer get_avail();
void free_avail(pointer s);
pointer get_node(int s);
void free_node(pointer p, halfword s);
void flush_list(pointer p);
void flush_node_list(pointer p);
pointer id_lookup(int j, int l);
void primitive(str_number s, halfword c, halfword o);
void flush_token_list(pointer p);
void show_token_list(int p, int q, int l, int null_tally);
void print_capsule();
void token_recycle();
void initialize();
void show_context();
void begin_token_list(pointer p, quarterword t);
void end_token_list();
pointer cur_tok();
void back_input();
void back_error();
void ins_error();
void runaway();
void pack_file_name(str_number n, str_number a, str_number e);
str_number make_name_string();
str_number a_make_name_string(alpha_file);
str_number b_make_name_string(byte_file);
str_number w_make_name_string(word_file);
void pack_job_name(str_number s);
void open_log_file();
void recycle_value(pointer p);
void fix_design_size();
int dimen_out(scaled x);
void init_prim();
void init_tab();
void debug_help();
void close_files_and_terminate();
void print_exp(pointer p, small_number verbosity);
pointer stash_cur_exp();
void prompt_file_name(str_number s, str_number e);
void print_file_name(int n, int a, int e);
void fatal_error(str_number s);
scaled round_fraction(fraction x);
int round_unscaled(scaled x);
int floor_unscaled(scaled x);
scaled floor_scaled(scaled x);
void make_exp_copy(pointer p);
void fix_check_sum();
void unstash_cur_exp(pointer p);
void print_op(quarterword c);
void print_type(small_number t);
void print_dp(small_number t, pointer p, small_number verbosity);
pointer new_num_tok(scaled v);
void begin_diagnostic();
void end_diagnostic(bool blank_line);
void print_dependency(pointer p, small_number t);
bool interesting(pointer p);
void toss_pen(pointer p);
void toss_knot_list(pointer p);
void toss_edges(pointer h);
void make_known(pointer p, pointer q);
fraction make_fraction(int p, int q);
void fix_dependencies();
pointer p_plus_fq(pointer p, int f, pointer q, small_number t, small_number tt);
pointer p_over_v(pointer p, scaled v, small_number t0, small_number t1);
pointer copy_edges(pointer h);
pointer copy_path(pointer p);
void encapsulate(pointer p);
pointer copy_dep_list(pointer p);
pointer single_dependency(pointer p);
pointer const_dependency(scaled v);
void val_too_big(scaled x);
int slow_add(int x, int y);
int take_fraction(int q, fraction f);
int take_scaled(int q, scaled f);
void new_dep(pointer q, pointer p);
void clear_for_error_prompt();
void end_file_reading();
void get_next();
void print_cmd_mod(int c, int m);
void init_big_node(pointer p);
void install(pointer r, pointer q);
void print_path(pointer h, str_number s, bool nuline);
void print_pen(pointer p, str_number s, bool nuline);
int skimp(int m);
void print_diagnostic(str_number s, str_number t, bool nuline);
void print_weight(pointer q, int x_off);
scaled threshold(int m);
void unskew(scaled x, scaled y, small_number octant);
void tfm_two(int x);
void tfm_four(int x);
void tfm_qqqq(four_quarters x);
void tfm_out(int c);
void tfm_warning(small_number m);
void write_gf(gf_index a, gf_index b);
void gf_swap();
void gf_four(int x);
void gf_two(int x);
void gf_three(int x);
int min_cover(scaled d);
pointer sort_in(scaled v);
void n_sin_cos(angle z);
int pyth_add(int a, int b);
void init_randoms(scaled seed);
void new_randoms();
void start_input();
bool load_base_file();
bool open_base_file();
void scan_file_name();
void begin_file_reading();
void firm_up_the_line();
void flush_error(scaled v);
void put_get_error();
void put_get_flush_error(scaled v);
void flush_cur_exp(scaled v);
void do_statement();
void get_x_next();
void expand();
void conditional();
void macro_call(pointer def_ref, pointer arg_list, pointer macro_name);
void begin_iteration();
void check_colon();
void change_if_limit(small_number l, pointer p);
void get_boolean();
void show_cmd_mod(int c, int m);
void missing_err(str_number s);
void pass_text();
void scan_primary();
void stop_iteration();
void resume_iteration();
void disp_err(pointer p, str_number s);
void bad_exp(str_number s);
void check_mem(bool print_locs);
void print_macro_name(pointer a, pointer n);
void print_arg(pointer q, int n, pointer b);
void scan_expression();
void scan_suffix();
void scan_text_arg(pointer l_delim, pointer r_delim);
void scan_secondary();
void binary_mac(pointer p, pointer c, pointer n);
void scan_tertiary();
void bad_for(str_number s);
void get_symbol();
void get_clear_symbol();
void clear_symbol(pointer p, bool saving);
void flush_below_variable(pointer p);
pointer scan_toks(command_code terminator, pointer subst_list, pointer tail_end, small_number suffix_count);
void stack_argument(pointer p);
bool check_outer_validity();
scaled round_decimals(small_number k);
void stash_in(pointer p);
void check_delimiter(pointer l_delim, pointer r_delim);
void unsave();
void frac_mult(scaled n, scaled d);
void do_binary(pointer p, quarterword c);
void bad_binary(pointer p, quarterword c);
pointer tarnished(pointer p);
void do_nullary(quarterword c);
void do_unary(quarterword c);
void back_expr();
void bad_subscript();
pointer find_variable(pointer t);
void obliterated(pointer q);
void new_root(pointer x);
pointer new_knot();
pointer copy_knot(pointer p);
void known_pair();
small_number scan_direction();
void materialize_pen();
void make_choices(pointer knots);
void dep_mult(pointer p, int v, bool v_is_scaled);
void do_assignment();
void do_equation();
void init_gf();
void gf_string(str_number s, str_number t);
angle n_arg(int x, int y);
void solve_choices(pointer p, pointer q, halfword n);
scaled norm_rand();
scaled unif_rand(scaled x);
small_number und_type(pointer p);
scaled m_log(scaled x);
int ab_vs_cd(int a, int b, int c, int d);
pointer new_structure(pointer p);
void set_controls(pointer p, pointer q, int k);
fraction curl_ratio(scaled gamma, scaled a_tension, scaled b_tension);
void pack_buffered_name(small_number n, int a, int b);
void store_base_file();
void dump_wd(memory_word memword);
void dump_int(int n);
void dump_hh(two_halves halves);
void dump_qqqq(four_quarters fq);
void undump_wd(memory_word *memword);
void undump_int(int *anint);
void undump_hh(two_halves *hh);
void undump_qqqq(four_quarters *qqqq);
void sort_avail();
void make_op_def();
void do_type_declaration();
void scan_def();
void do_protection();
void def_delims();
void do_random_seed();
void save_variable(pointer q);
void do_interim();
void do_new_internal();
void do_let();
void do_show_whatever();
void do_add_to();
void do_ship_out();
void do_display();
void do_open_window();
void do_cull();
void do_message();
void do_tfm_command();
void do_special();
fraction max_coef(pointer p);
pointer p_times_v(pointer p, int v, small_number t0, small_number t1, bool v_is_scaled);
void dep_finish(pointer v, pointer q, small_number t);
void do_show_token();
void do_show_stats();
void disp_var(pointer p);
void do_show_var();
void do_show_dependencies();
void do_show();
void disp_token();
void save_internal(halfword q);
void flush_variable(pointer p, pointer t, bool discard_suffixes);
pointer scan_declared_variable();
void nonlinear_eq(int v, pointer p, bool flush_p);
void ring_merge(pointer p, pointer q);
void pair_to_path();
void make_eq(pointer lhs);
void try_eq(pointer l, pointer r);
void linear_eq(pointer p, small_number t);
pointer p_plus_q(pointer p, pointer q, small_number t);
fraction velocity(fraction st, fraction ct, fraction sf, fraction cf, scaled t);
pointer p_with_x_becoming_q(pointer p, pointer x, pointer q, small_number t);
void add_or_subtract(pointer p, pointer q, quarterword c);
void negate_edges(pointer h);
void merge_edges(pointer h);
void take_part(quarterword c);
void edge_prep(int ml, int mr, int nl, int nr);
void fix_offset();
void print_known_or_unknown_type(small_number t, int v);
void negate_dep_list(pointer p);
void bad_unary(quarterword c);
pointer htap_ypoc(pointer p);
bool nice_pair(int p, quarterword t);
pointer make_path(pointer pen_head);
int total_weight(pointer h);
scaled path_length();
pointer make_spec(pointer h, scaled safety_margin, int tracing);
void new_boundary(pointer p, small_number octant);
void  print_spec(str_number s);
void remove_cubic(pointer p);
void skew(scaled x, scaled y, small_number octant);
void test_known(quarterword c);
void str_to_num(quarterword c);
void quadrant_subdivide();
void octant_subdivide();
void abnegate(scaled x, scaled y, small_number octant_before, small_number octant_after);
fraction crossing_point(int a, int b, int c);
void xy_round();
void split_cubic(pointer p, fraction t, scaled xq, scaled yq);
void diag_round();
scaled compromise(scaled u, scaled v);
scaled good_val(scaled b, scaled o);
void make_safe();
void before_and_after(scaled b, scaled a, pointer p);
pointer trivial_knot(scaled x, scaled y);
scaled m_exp(scaled x);
scaled square_rt(scaled x);
void big_trans(pointer p, quarterword c);
void bilin3(pointer p, scaled t, scaled v, scaled u, scaled delta);
void bilin2(pointer p, pointer t, scaled v, pointer u, pointer q);
void add_mult_dep(pointer p, scaled v, pointer r);
void bilin1(pointer p, scaled t, pointer q, scaled u, scaled delta);
void chop_string(pointer p);
void cat(pointer p);
void chop_path(pointer p);
void dep_div(pointer p, scaled v);
void edges_trans(pointer p, quarterword c);
void find_point(scaled v, quarterword c);
void hard_times(pointer);
void pair_value(scaled x, scaled y);
void path_intersection(pointer h, pointer hh);
int pyth_sub(int a, int b);
void path_trans(pointer p, quarterword c);
void set_up_offset(pointer p);
void set_up_direction_time(pointer p);
scaled find_direction_time(scaled x, scaled y, pointer h );
void find_offset(scaled x, scaled y, pointer p);
void set_up_trans(quarterword c);
void trans(pointer p, pointer q);
void set_up_known_trans(quarterword c);
void check_equals();
void cubic_intersection(pointer p, pointer pp);
bool scan_with();
void fill_envelope(pointer spec_head);
void make_moves(scaled xx0, scaled xx1, scaled xx2, scaled xx3, scaled yy0, scaled yy1, scaled yy2, scaled yy3, small_number xi_corr, small_number eta_corr);
void begin_edge_tracing();
void trace_a_corner();
void end_edge_tracing();
void skew_line_edges(pointer p, pointer w, pointer ww);
void line_edges(scaled x0,scaled y0,scaled x1,scaled y1);
void end_round(scaled x, scaled y);
void fill_spec(pointer h);
void trace_new_edge(pointer r, int n);
void move_to_edges(int m0, int n0, int m1, int n1);
void smooth_moves(int b, int t);
void find_edges_var(pointer t);
void offset_prep(pointer c, pointer h);
void dual_moves(pointer h, pointer p, pointer q);
void fin_offset_prep(pointer p, halfword k, pointer w, int x0, int x1, int x2, int y0, int y1, int y2,
						bool rising, int n);
void split_for_offset(pointer p, fraction t);
void print_word(memory_word w);
void search_mem(pointer p);
void print_strange(str_number s);
pointer make_pen(pointer h);
void dup_offset(pointer w);
pointer make_ellipse(scaled major_axis, scaled minor_axis, angle theta);
pointer id_transform();
void xy_swap_edges();
void sort_edges(pointer h);
void x_reflect_edges();
void y_reflect_edges();
void y_scale_edges(int s);
void x_scale_edges(int s);
void cull_edges(int w_lo,int w_hi,int w_out,int w_in);
bool get_pair(command_code c);
void disp_edges(window_number k);
void paint_row(screen_row r, pixel_color b, trans_spec& a, screen_col n);
void blank_rectangle(screen_col left_col,screen_col right_col, screen_row top_row,screen_row bot_row);
bool init_screen();
void update_screen();
void open_a_window(window_number k,scaled r0, scaled c0,scaled r1,scaled c1,scaled x,scaled y);
void ship_out(eight_bits c);
scaled tfm_check(small_number m);
eight_bits get_code();
void set_tag(halfword c, small_number t, halfword r);
void gf_paint(int d);
void gf_boc(int min_m, int max_m, int min_n, int max_n);




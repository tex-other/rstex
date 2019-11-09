/*

Copyright (C) 2018 by Richard Sandberg.

This is the Windows specific version of rsGFtoDVI.
No Unix version exists yet, this should be easy to add though.

*/

#include "rsGFtoDVI.h"
#include <io.h>

int myabs(int x)
{
	// overflow check
	if (x == INT_MIN && -(INT_MIN + 1) == INT_MAX) {
		printf("Overflow myabs.\n");
		exit(1);
	}
	///////////////////////

	return x >= 0 ? x : -x;
}

bool myodd(int c)
{
	return ((c % 2) != 0);
}



///////////////////////////////////////////////////////////////////////////
// System specific addition for paths on windows

void copypath(char *s1, char *s2, int n)
{
	while ((*s1++ = *s2++) != 0) {
		if (--n == 0) {
			fprintf(stderr, "! Environment search path is too big\n");
			*--s1 = 0;
			return;
		}
	}
}

void set_paths()
{
	char *envpath;
	if ((envpath = getenv("TEXINPUTS")) != NULL)
		copypath(input_path, envpath, MAX_INPUT_CHARS);
	if ((envpath = getenv("TEXFONTS")) != NULL)
		copypath(font_path, envpath, MAX_INPUT_CHARS);
	if ((envpath = getenv("TEXFORMATS")) != NULL)
		copypath(format_path, envpath, MAX_INPUT_CHARS);
	if ((envpath = getenv("TEXPOOL")) != NULL)
		copypath(pool_path, envpath, MAX_INPUT_CHARS);
}

void pack_real_name_of_file(char **cpp)
{
	char *p;
	char *real_name;

	real_name = &real_name_of_file[1];
	if ((p = *cpp) != NULL) {
		while ((*p != ';') && (*p != 0)) {
			*real_name++ = *p++;
			if (real_name == &real_name_of_file[file_name_size])
				break;
		}
		if (*p == 0) *cpp = NULL;
		else *cpp = p + 1;
		*real_name++ = '\\';
	}
	p = name_of_file.get_c_str();
	
	while (*p != 0) {
		if (real_name >= &real_name_of_file[file_name_size]) {
			fprintf(stderr, "! Full file name is too long\n");
			break;
		}
		*real_name++ = *p++;
	}
	*real_name = 0;
	
}

bool test_access(int filepath)
{
	bool ok;
	char *cur_path_place;

	switch (filepath) {
	case no_file_path: cur_path_place = NULL; break;
	case input_file_path: 
	case read_file_path:
		cur_path_place = input_path; break;
	case font_file_path: cur_path_place = font_path; break;
	case format_file_path: cur_path_place = format_path; break;
	case pool_file_path: cur_path_place = pool_path; break;
	default:
		fprintf(stderr, "! This should not happen, test_access\n");
		exit(1);
		break;
	}
	if (name_of_file[1] == '\\' || (isalpha(name_of_file[1]) && name_of_file[2] == ':'))
		cur_path_place = NULL;
	do {
		pack_real_name_of_file(&cur_path_place);
		if (_access(real_name_of_file.get_c_str(), 0) == 0)
			ok = true;
		else
			ok = false;
	} while (!ok && cur_path_place != NULL);

	return ok;
}

///////////////////////////////////////////////////////////////////////////










void jump_out()
{
	// goto final_end
	exit(0);
}

// 3
void initialize()
{
	// 3
	int i,j,m,n;

	fprintf(stdout, "%s\n", banner);
	// 13
	xchr[040]=' ';
	xchr[041]='!';
	xchr[042]='\"';
	xchr[043]='#';
	xchr[044]='$';
	xchr[045]='%';
	xchr[046]='&';
	xchr[047]='\'';
	xchr[050]='(';
	xchr[051]=')';
	xchr[052]='*';
	xchr[053]='+';
	xchr[054]=',';
	xchr[055]='-';
	xchr[056]='.';
	xchr[057]='/';
	xchr[060]='0';
	xchr[061]='1';
	xchr[062]='2';
	xchr[063]='3';
	xchr[064]='4';
	xchr[065]='5';
	xchr[066]='6';
	xchr[067]='7';
	xchr[070]='8';
	xchr[071]='9';
	xchr[072]=':';
	xchr[073]=';';
	xchr[074]='<';
	xchr[075]='=';
	xchr[076]='>';
	xchr[077]='?';
	xchr[0100]='@';
	xchr[0101]='A';
	xchr[0102]='B';
	xchr[0103]='C';
	xchr[0104]='D';
	xchr[0105]='E';
	xchr[0106]='F';
	xchr[0107]='G';
	xchr[0110]='H';
	xchr[0111]='I';
	xchr[0112]='J';
	xchr[0113]='K';
	xchr[0114]='L';
	xchr[0115]='M';
	xchr[0116]='N';
	xchr[0117]='O';
	xchr[0120]='P';
	xchr[0121]='Q';
	xchr[0122]='R';
	xchr[0123]='S';
	xchr[0124]='T';
	xchr[0125]='U';
	xchr[0126]='V';
	xchr[0127]='W';
	xchr[0130]='X';
	xchr[0131]='Y';
	xchr[0132]='Z';
	xchr[0133]='[';
	xchr[0134]='\\';
	xchr[0135]=']';
	xchr[0136]='^';
	xchr[0137]='_';
	xchr[0140]='`';
	xchr[0141]='a';
	xchr[0142]='b';
	xchr[0143]='c';
	xchr[0144]='d';
	xchr[0145]='e';
	xchr[0146]='f';
	xchr[0147]='g';
	xchr[0150]='h';
	xchr[0151]='i';
	xchr[0152]='j';
	xchr[0153]='k';
	xchr[0154]='l';
	xchr[0155]='m';
	xchr[0156]='n';
	xchr[0157]='o';
	xchr[0160]='p';
	xchr[0161]='q';
	xchr[0162]='r';
	xchr[0163]='s';
	xchr[0164]='t';
	xchr[0165]='u';
	xchr[0166]='v';
	xchr[0167]='w';
	xchr[0170]='x';
	xchr[0171]='y';
	xchr[0172]='z';
	xchr[0173]='{';
	xchr[0174]='|';
	xchr[0175]='}';
	xchr[0176]='~';

	// 14
	for (i=0; i <= 037; i++) xchr[i]='?';
	for (i=0177; i <= 0377; i++) xchr[i]='?';

	// 15
	for (i=first_text_char; i<= last_text_char; i++) xord[i]=/* */32;
	for (i=1; i<= 0377; i++) xord[xchr[i]]=i;
	xord['?']=/*?*/63;

	// 54
	fmem_ptr=0;

	// 97
	interaction=false; fonts_not_loaded=true;
	font_name[title_font]=default_title_font;
	font_name[label_font]=default_label_font;
	font_name[gray_font]=default_gray_font;
	font_name[slant_font]=null_string;
	font_name[logo_font]=logo_font_name;
	for (k=title_font; k<= logo_font; k++)
	{ 
		font_area[k]=null_string; font_at[k]=0;
	}

	// 103
	total_pages=0; max_v=0; max_h=0; last_bop=-1;

	// 106
	half_buf=dvi_buf_size / 2; dvi_limit=dvi_buf_size; dvi_ptr=0;
	dvi_offset=0;

	// 118
	dummy_info.b0=qi(0); dummy_info.b1=qi(0); dummy_info.b2=qi(0);
	dummy_info.b3=qi(0);

	// 126
	c[1]=1; d[1]=2; two_to_the[0]=1; m=1;
	for (k=1; k<= 13; k++) two_to_the[k]=2*two_to_the[k-1];
	for (k=2; k<= 6; k++)  
		//@<Add a full set of |k|-bit characters@>;
	{ 
		n=two_to_the[k-1];
		for (j=0; j <= n-1; j++)
		{ 
			incr(m); c[m]=m; d[m]=n+n;
		}
	}
		//@<Add a full set of |k|-bit characters@>;
	for (k=7; k<= 12; k++) 
		//@<Add special |k|-bit characters of the form \.{X..XO..O}@>;
	{ 
		n=two_to_the[k-1];
		for (j=k; j >= 1; j--)
		{ 
			incr(m); d[m]=n+n;
			if (j==k) c[m]=n;
			else c[m]=c[m-1]+two_to_the[j-1];
		}
	}
		//@<Add special |k|-bit characters of the form \.{X..XO..O}@>;

	// 142
	yy[0]=-010000000000; yy[end_of_list]=010000000000;


}

void update_terminal()
{
	fflush(stdout);
}

// 113
void typeset(eight_bits c)
{
	if (c>=128) dvi_out(set1);
	dvi_out(c);
}


// 116
void hbox(str_number s, internal_font_number f, bool send_it)
{
	pool_pointer k,end_k,max_k; //{indices into |str_pool|}
	four_quarters i,j; //{font information words}
	int cur_l; //:0..256; //{character to the left of the ``cursor''}
	int cur_r; //:min_quarterword..non_char;// {character to the right of the ``cursor''}
	int bchar; //:min_quarterword..non_char; //{right boundary character}
	int stack_ptr; //:0..lig_lookahead; //{number of entries on |lig_stack|}
	font_index l; //{pointer to lig/kern instruction}
	scaled kern_amount; //{extra space to be typeset}
	eight_bits hd; //{height and depth indices for a character}
	scaled x; //{temporary register}
	ASCII_code save_c; //{character temporarily blanked out}
	box_width=0; box_height=0; box_depth=0;
	k=str_start[s]; max_k=str_start[s+1];
	save_c=str_pool[max_k]; str_pool[max_k]=/* */32;
	while (k<max_k)
	{ 
		if (str_pool[k]==/* */32) 
			//@<Typeset a space in font |f| and advance~|k|@>
		{ 
			box_width=box_width+space(f);
			if (send_it)
			{ 
				dvi_out(right4); dvi_four(space(f));
			}
			incr(k);
		}
			//@<Typeset a space in font |f| and advance~|k|@>
		else { 
			end_k=k;
			do { 
				incr(end_k); 
			} while (!(str_pool[end_k]==/* */32));
			kern_amount=0; cur_l=256; stack_ptr=0; bchar=font_bchar[f];
			set_cur_r; suppress_lig=false;
		mycontinue: 
			//@<If there's a ligature or kern at the cursor position, update the cursor data structures, possibly advancing~|k|; continue until the cursor wants to move right@>;
			if(cur_l<font_bc[f] || cur_l>font_ec[f])
			{ 
				i=dummy_info;
				if (cur_l==256) l=bchar_label[f]; else l=non_address;
			}
			else { 
				i=char_info(f,cur_l);
				if (char_tag(i)!=lig_tag) l=non_address;
				else { 
					l=lig_kern_start(f,i); j=font_info[l].qqqq;
					if (skip_byte(j)>stop_flag) l=lig_kern_restart(f,j);
				}
			}
			if (suppress_lig) suppress_lig=false;
			else while (l<qi(kern_base[f]))
			{ 
				j=font_info[l].qqqq;
				if (next_char(j)==cur_r) 
					if (skip_byte(j)<=stop_flag)
						if (op_byte(j)>=kern_flag)
						{ 
							kern_amount=char_kern(f,j); goto done;
						}
						else 
							//@<Carry out a ligature operation, updating the cursor structure and possibly advancing~|k|; |goto continue| if the cursor doesn't advance, otherwise |goto done|@>;
						{
							switch(op_byte(j)) {
								case 1: case 5:cur_l=qo(rem_byte(j)); break;
								case 2: case 6:
									cur_r=rem_byte(j);
									if (stack_ptr==0)
									{ 
										stack_ptr=1;
										if (k<end_k) incr(k); //{a non-space character is consumed}
										else bchar=non_char; //{the right boundary character is consumed}
									}
									lig_stack[stack_ptr]=cur_r;
									break;
								case 3: case 7: case 11:
									cur_r=rem_byte(j); incr(stack_ptr); lig_stack[stack_ptr]=cur_r;
									if (op_byte(j)==11) suppress_lig=true;
									break;
								default: 
									cur_l=qo(rem_byte(j));
									if (stack_ptr>0) pop_stack;
									else if (k==end_k) goto done;
									else {
										incr(k); set_cur_r;
									}
									break;
							}
							if (op_byte(j)>3) goto done;
							goto mycontinue;
						}
							//@<Carry out a ligature operation, updating the cursor structure and possibly advancing~|k|; |goto continue| if the cursor doesn't advance, otherwise |goto done|@>;
				if (skip_byte(j)>=stop_flag) goto done;
				l=l+skip_byte(j)+1;
			}
		done:
			//@<If there's a ligature or kern at the cursor position, update the cursor data structures, possibly advancing~|k|; continue until the cursor wants to move right@>;

			//@<Typeset character |cur_l|, if it exists in the font; also append an optional kern@>;
			if (char_exists(i))
			{ 
				box_width=box_width+char_width(f,i)+kern_amount;
				hd=height_depth(i);
				x=char_height(f,hd);
				if (x>box_height) box_height=x;
				x=char_depth(f,hd);
				if (x>box_depth) box_depth=x;
				if (send_it)
				{ 
					typeset(cur_l);
					if (kern_amount!=0)
					{ 
						dvi_out(right4); dvi_four(kern_amount);
					}
				}
				kern_amount=0;
			}
			//@<Typeset character |cur_l|, if it exists in the font; also append an optional kern@>;

			//@<Move the cursor to the right and |goto continue|, if there's more work to do in the current word@>;
			cur_l=qo(cur_r);
			if (stack_ptr>0)
			{ 
				pop_stack; goto mycontinue;
			}
			if (k<end_k)
			{ 
				incr(k); set_cur_r; goto mycontinue;
			}
			//@<Move the cursor to the right and |goto continue|, if there's more work to do in the current word@>;
		} //{now |k=end_k|}
	}
	str_pool[max_k]=save_c;
}

str_number make_string() //{current string enters the pool}
{ 
	if (str_ptr==max_strings)
		abort("Too many labels!");
	incr(str_ptr); str_start[str_ptr]=pool_ptr;
	return str_ptr-1;
}


// 47
void open_gf_file() //{prepares to read packed bytes in |gf_file|}
{ 
	if (test_access(no_file_path)) {
		gf_file = fopen(real_name_of_file.get_c_str(), "rb");
		cur_loc=0;
	} else {
		char temp[file_name_size+50];
		sprintf(temp, "Can\'t open GF file %s", real_name_of_file.get_c_str());
		abort(temp);
	}
}

void open_tfm_file() //{prepares to read packed bytes in |tfm_file|}
{
	if (test_access(font_file_path)) {
		tfm_file = fopen(real_name_of_file.get_c_str(), "rb");
		cur_loc=0; // Not sure why this change is necessary for SPARC
		// temp_int:=tfm_file^; {prime the pump}
		// if temp_int<0 then temp_int:=temp_int+256;
	}
	else {
		char temp[file_name_size+50];
		sprintf(temp, "Can\'t open TFM file %s", real_name_of_file.get_c_str());
		abort(temp);		
	}
}

void open_dvi_file() //{prepares to write packed bytes in |dvi_file|}
{

	dvi_file = fopen(name_of_file.get_c_str(), "wb");
	if (!dvi_file) {
		char temp[file_name_size+50];
		sprintf(temp, "Can\'t write on DVI file %s", real_name_of_file.get_c_str());
		abort(temp);
	}

}


void write_dvi(dvi_index a, dvi_index b)
{
	dvi_index k;
	for (k=a; k<=b; k++) fputc(dvi_buf[k], dvi_file);
}

void dvi_swap() //{outputs half of the buffer}
{ 
	if (dvi_limit==dvi_buf_size)
	{ 
		write_dvi(0,half_buf-1); dvi_limit=half_buf;
		dvi_offset=dvi_offset+dvi_buf_size; dvi_ptr=0;
	}
	else { 
		write_dvi(half_buf,dvi_buf_size-1); dvi_limit=dvi_buf_size;
	}
}


void dvi_four(int x)
{
	if (x >= 0) dvi_out(x / 0100000000);
	else { 
		x=x+010000000000;
		x=x+010000000000;
		dvi_out((x / 0100000000) + 128);
	}
	x=x % 0100000000; dvi_out(x / 0200000);
	x=x % 0200000; dvi_out(x / 0400);
	dvi_out(x % 0400);
}

// 89
void begin_name()
{
	area_delimiter = 0;
	ext_delimiter = 0;
}

// 90
bool more_name(ASCII_code c)
{
	if (c == /* */32) return false;
	else {
		if (c == /*/*/47) {
			area_delimiter = pool_ptr;
			ext_delimiter = 0;
		}
		else if (c == /*.*/46 && ext_delimiter == 0)
			ext_delimiter = pool_ptr;
		str_room(1); append_char(c);
		return true;
	}
}

// 91
void end_name()
{
	if (str_ptr + 3 > max_strings)
		abort("Too many strings!");
	if (area_delimiter == 0)
		cur_area = null_string;
	else {
		cur_area = str_ptr; incr(str_ptr); str_start[str_ptr] = area_delimiter+1;
	}
	if (ext_delimiter == 0) {
		cur_ext = null_string; cur_name = make_string();
	}
	else {
		cur_name = str_ptr;
		incr(str_ptr);
		str_start[str_ptr] = ext_delimiter;
		cur_ext = make_string();
	}
}

// 92
void pack_file_name(str_number n, str_number a, str_number e)
{
	int k; //{number of positions filled in |name_of_file|}
	ASCII_code c; //{character being packed}
	int j; //{index into |str_pool|}
	int name_length; //:0..file_name_size; //{number of characters packed}
	k=0;
	for (j=str_start[a]; j <= str_start[a+1]-1; j++) append_to_name(str_pool[j]);
	for (j=str_start[n]; j <= str_start[n+1]-1; j++) append_to_name(str_pool[j]);
	for (j=str_start[e]; j <= str_start[e+1]-1; j++) append_to_name(str_pool[j]);
	if (k<=file_name_size) name_length=k; else name_length=file_name_size;
	name_of_file[k+1] = 0;
}


int get_byte() //{returns the next byte, unsigned}
{
	eight_bits b;
	int c;
	c = fgetc(gf_file);
	if (c == EOF)
		return 0;
	
	b = c;
	incr(cur_loc); 
	return b;
}

int get_two_bytes() //{returns the next two bytes, unsigned}
{
	eight_bits a, b;
	a = fgetc(gf_file); b = fgetc(gf_file);
	cur_loc=cur_loc+2;
	return a*256+b;
}

int get_three_bytes() //{returns the next three bytes, unsigned}
{
	eight_bits a,b,c;
	a = fgetc(gf_file); b = fgetc(gf_file); c = fgetc(gf_file);
	cur_loc=cur_loc+3;
	return (a*256+b)*256+c;
}

int signed_quad() //{returns the next four bytes, signed}
{
	eight_bits a,b,c,d;
	a = fgetc(gf_file); b = fgetc(gf_file); c = fgetc(gf_file); d = fgetc(gf_file);
	cur_loc=cur_loc+4;
	if (a<128) return ((a*256+b)*256+c)*256+d;
	else return (((a-256)*256+b)*256+c)*256+d;
}


scaled get_yyy()
{
	scaled v; //{value just read}
	if (cur_gf != yyy) return 0;
	else { 
		v=signed_quad(); cur_gf=get_byte(); return v;
	}
}


void input_ln() // inputs a line from the terminal
{
	update_terminal();
	line_length=0;
	int c;
	while ((c = fgetc(term_in)) != '\n' && c != EOF && line_length < terminal_line_length) {
		buffer[line_length] = xord[c]; incr(line_length);
	}

}


void first_string(int c)
{
	if (str_ptr != c) abort("?"); // {internal consistency check
	while (l > 0) {
		append_char(buffer[l]); decr(l);
	}
	incr(str_ptr); str_start[str_ptr] = pool_ptr;
}

// 138
void slant_complaint(float r)
{
	if (fabs(r-slant_reported)>0.001)
	{
		fprintf(stdout, "\nSorry, I can\'t make diagonal rules of slant %10.5f!", r);
		slant_reported=r;
	}
}

void start_gf(int argc, char *argv[])
{
	if (argc > 2) abort("Usage: gftodvi [GF-file]");
	if (argc == 1) {
		printf("\nGF file name: "); input_ln();
	}
	else {
		int cc=0;
		line_length=0;
		while (cc < file_name_size-1 && argv[1][cc] != ' ') incr(cc);
		while (cc < file_name_size-1 && line_length < terminal_line_length && argv[1][cc] != ' ') {
			buffer[line_length] = xord[argv[1][cc]];
			incr(line_length);
			incr(cc);
		}
	}

	buf_ptr=0; buffer[line_length]=/*?*/63;
	while (buffer[buf_ptr] == /* */32) incr(buf_ptr);
	if (buf_ptr<line_length)
	{ 
		//@<Scan the file name in the buffer@>;
		if (buffer[line_length-1]==/*/*/47)
		{ 
			interaction=true; decr(line_length);
		}
		begin_name();
		loop { 
			if (buf_ptr==line_length) goto done;
			if (!more_name(buffer[buf_ptr])) goto done;
			incr(buf_ptr);
		}
	done:
		end_name();
		//@<Scan the file name in the buffer@>;

		if (cur_ext==null_string) cur_ext=gf_ext;
		pack_file_name(cur_name,cur_area,cur_ext); open_gf_file();
	}

	job_name=cur_name; pack_file_name(job_name,null_string,dvi_ext);
	open_dvi_file();
}

keyword_code interpret_xxx()
{

	int k; //{number of bytes in an \\{xxx} command}
	int j; //{number of bytes read so far}
	int l; //:0..longest_keyword; {length of keyword to check}
	keyword_code m; //{runs through the list of known keywords}
	int n1; //:0..longest_keyword; {buffered character being checked}
	pool_pointer n2; //{pool character being checked}
	keyword_code c; //{the result to return}
	c=no_operation; cur_string=null_string;
	switch(cur_gf) {
		case no_op:
			goto done;
			break;
		case yyy:
			k=signed_quad(); 
			goto done;
			break;
		case xxx1:
			k=get_byte();
			break;
		case xxx2:
			k=get_two_bytes();
			break;
		case xxx3:
			k=get_three_bytes();
			break;
		case xxx4:
			k=signed_quad();
			break;
	} //{there are no other cases}
	//@<Read the next |k| characters of the \.{GF} file; change |c| and |goto done| if a keyword is recognized@>;
	j=0;
	if (k<2) goto not_found;
	loop { 
		l=j;
		if (j==k) goto done1;
		if (j==longest_keyword) goto not_found;
		incr(j); buffer[j]=get_byte();
		if (buffer[j]==/* */32) goto done1;
	}
done1:
	//@<If the keyword in |buffer[1..l]| is known, change |c| and |goto done|@>;
	for (m=null_string; m<= max_keyword; m++) 
		if (length(m)==l)
		{ 
			n1=0; n2=str_start[m];
			while (n1<l && buffer[n1+1] == str_pool[n2])
			{ 
				incr(n1); incr(n2);
			}
			if (n1==l)
			{ 
				c=m;
				if (m==null_string)
				{ 
					incr(j); label_type=get_byte();
				}
				str_room(k-j);
				while (j<k)
				{ 
					incr(j); append_char(get_byte());
				}
				cur_string=make_string(); goto done;
			}
		}
	//@<If the keyword in |buffer[1..l]| is known, change |c| and |goto done|@>;
not_found: 
	while (j<k)
	{ 
		incr(j); cur_gf=get_byte();
	}
	//@<Read the next |k| characters of the \.{GF} file; change |c| and |goto done| if a keyword is recognized@>;
done: 
	cur_gf=get_byte(); return c;
}

void read_tfm_word()
{ 
	b0 = fgetc(tfm_file); b1 = fgetc(tfm_file);
	b2 = fgetc(tfm_file); b3 = fgetc(tfm_file);
}

// 85
void skip_nop()
{
	int k; //{number of bytes in an \\{xxx} command}
	int j; //{number of bytes read so far}
	switch(cur_gf) {
		case no_op:goto done; break;
		case yyy:
			k=signed_quad(); goto done;
			break;
		case xxx1:k=get_byte(); break;
		case xxx2:k=get_two_bytes(); break;
		case xxx3:k=get_three_bytes(); break;
		case xxx4:k=signed_quad(); break;
	} //{there are no other cases}
	for (j=1; j<=k; j++) cur_gf=get_byte();
done: 
	cur_gf=get_byte();
}

void read_font_info(int f, scaled s) // {input a \.{TFM} file}
{
	font_index k; //{index into |font_info|}
	int lf,lh,bc,ec,nw,nh,nd,ni,nl,nk,ne,np; //:0..65535;
	//{sizes of subfiles}
	int bch_label; //{left boundary label for ligatures}
	int bchar; //:0..256; //{right boundary character for ligatures}
	four_quarters qw; scaled sw; //{accumulators}
	scaled z; //{the design size or the ``at'' size}
	int alpha; int beta; //:1..16;
	//{auxiliary quantities used in fixed-point multiplication}

	//@<Read and check the font data; |abend| if the \.{TFM} file is
	//@<Read the {\.{TFM}} size fields@>;
	read_two_halves(lf,lh);
	read_two_halves(bc,ec);
	if (bc>ec+1 || ec>255) abend;
	if (bc>255) //{|bc=256| and |ec=255|}
	{ 
		bc=1; ec=0;
	}
	read_two_halves(nw,nh);
	read_two_halves(nd,ni);
	read_two_halves(nl,nk);
	read_two_halves(ne,np);
	if (lf != 6+lh+(ec-bc+1)+nw+nh+nd+ni+nl+nk+ne+np) abend;
	
	//@<Read the {\.{TFM}} size fields@>;


	//@<Use size fields to allocate font information@>;
	lf=lf-6-lh; //{|lf| words should be loaded into |font_info|}
	if (np<8) lf=lf+8-np; //{at least eight parameters will appear}
	if (fmem_ptr+lf>font_mem_size) abort("No room for TFM file!");
	char_base[f]=fmem_ptr-bc;
	width_base[f]=char_base[f]+ec+1;
	height_base[f]=width_base[f]+nw;
	depth_base[f]=height_base[f]+nh;
	italic_base[f]=depth_base[f]+nd;
	lig_kern_base[f]=italic_base[f]+ni;
	kern_base[f]=lig_kern_base[f]+nl;
	exten_base[f]=kern_base[f]+nk;
	param_base[f]=exten_base[f]+ne;
	//@<Use size fields to allocate font information@>;


	//@<Read the {\.{TFM}} header@>;
	
	if (lh<2) abend;
	store_four_quarters(font_check[f]);
	read_tfm_word();
	if (b0>127) abend; //{design size must be positive}
	z=((b0*256+b1)*256+b2)*16+(b3 / 16);
	if (z<unity) abend;
	while (lh>2)
	{ 
		read_tfm_word(); decr(lh); //{ignore the rest of the header}
	}
	font_dsize[f]=z;
	if (s>0) z=s;
	font_size[f]=z;
	
	//@<Read the {\.{TFM}} header@>;


	//@<Read character data@>;
	for (k=fmem_ptr; k<=width_base[f]-1; k++)
	{ 
		store_four_quarters(font_info[k].qqqq);
		if (b0>=nw || b1 / 020 >= nh || b1 % 020 >= nd || b2 / 4 >= ni) abend;
		switch(b2 % 4) {
			case lig_tag: if (b3>=nl) abend; break;
			case ext_tag: if (b3>=ne) abend; break;
			case no_tag: case list_tag: do_nothing; break;
		} //{there are no other cases}
	}
	//@<Read character data@>;


	//@<Read box dimensions@>;

	//@<Replace |z| by $|z|^\prime$ and compute $\alpha,\beta$@>;
	alpha=16*z; beta=16;
	while (z >= 040000000)
	{ 
		z=z / 2; beta=beta / 2;
	}
	//@<Replace |z| by $|z|^\prime$ and compute $\alpha,\beta$@>;
	for (k=width_base[f]; k<=lig_kern_base[f]-1; k++)
		store_scaled(font_info[k].sc);
	if (font_info[width_base[f]].sc!=0) abend; //{\\{width}[0] must be zero}
	if (font_info[height_base[f]].sc!=0) abend; //{\\{height}[0] must be zero}
	if (font_info[depth_base[f]].sc!=0) abend; //{\\{depth}[0] must be zero}
	if (font_info[italic_base[f]].sc!=0) abend; //{\\{italic}[0] must be zero}
	//@<Read box dimensions@>;


	//@<Read ligature/kern program@>;
	bch_label=077777; bchar=256;
	if (nl>0)
	{ 
		for (k=lig_kern_base[f]; k<=kern_base[f]-1; k++)
		{ 
			store_four_quarters(font_info[k].qqqq);
			if (b0>stop_flag)
			{ 
				if (256*b2+b3>=nl) abend;
				if (b0==255) 
					if (k==lig_kern_base[f]) bchar=b1;
			}
			else { 
				if (b1!=bchar) check_byte_range(b1);
				if (b2<kern_flag) check_byte_range(b3);
				else if (256*(b2-128)+b3>=nk) abend;
			}
		}
		if (b0==255) bch_label=256*b2+b3;
	}
	for (k=kern_base[f]; k<=exten_base[f]-1; k++)
		store_scaled(font_info[k].sc);
	//@<Read ligature/kern program@>;


	//@<Read extensible character recipes@>;
	for (k=exten_base[f]; k<=param_base[f]-1; k++)
	{ 
		store_four_quarters(font_info[k].qqqq);
		if (b0!=0) check_byte_range(b0);
		if (b1!=0) check_byte_range(b1);
		if (b2!=0) check_byte_range(b2);
		check_byte_range(b3);
	}
	//@<Read extensible character recipes@>;


	//@<Read font parameters@>;
	for (k=1; k<= np; k++)
		if (k==1) //{the |slant| parameter is a pure number}
		{
			read_tfm_word();
			if (b0>127) sw=b0-256; else sw=b0;
			sw=sw*0400+b1; sw=sw*0400+b2;
			font_info[param_base[f]].sc=(sw*020)+(b3 / 020);
		}
	else store_scaled(font_info[param_base[f]+k-1].sc);
	for (k=np+1; k<= 8; k++) font_info[param_base[f]+k-1].sc=0;
	
	//@<Read font parameters@>;


	//@<Make final adjustments and |goto done|@>
	font_bc[f]=bc; font_ec[f]=ec;
	if (bch_label<nl) bchar_label[f]=bch_label+lig_kern_base[f];
	else bchar_label[f]=non_address;
	font_bchar[f]=qi(bchar);
	adjust(width_base); adjust(lig_kern_base);
	adjust(kern_base); adjust(exten_base);
	decr(param_base[f]);
	fmem_ptr=fmem_ptr+lf; goto done;
	//@<Make final adjustments and |goto done|@>


	//@<Read and check the font data; |abend| if the \.{TFM} file is
bad_tfm: 
	fprintf(stdout, "\nBad TFM file for");

	switch(f) {
		case title_font:abort("titles!");break;
		case label_font:abort("labels!");break;
		case gray_font:abort("pixels!");break;
		case slant_font:abort("slants!");break;
		case logo_font:abort("METAFONT logo!");break;
	} //{there are no other cases}
done: 
	;
	//{it might be good to close |tfm_file| now}
	fclose(tfm_file);
}
// 114
void dvi_scaled(float x)
{
	int n; //{an integer approximation to |10*x/unity|}
	int m; //{the integer part of the answer}
	int k; //{the number of digits in |m|}
	n=int(round(x/6553.6f));
	if (n<0)
	{ 
		dvi_out(/*-*/45); n=-n;
	}
	m=n / 10; k=0;
	do { 
		incr(k); buffer[k]=(m % 10)+/*0*/48; m=m / 10;
	} while (!(m==0));
	do { 
		dvi_out(buffer[k]); decr(k);
	} while (!(k==0));
	if (n % 10 != 0)
	{ 
		dvi_out(/*.*/46); dvi_out((n % 10)+/*0*/48);
	}
}


void dvi_font_def(internal_font_number f)
{
	int k; //{index into |str_pool|}
	dvi_out(fnt_def1);
	dvi_out(f);
	dvi_out(qo(font_check[f].b0));
	dvi_out(qo(font_check[f].b1));
	dvi_out(qo(font_check[f].b2));
	dvi_out(qo(font_check[f].b3));
	dvi_four(font_size[f]);
	dvi_four(font_dsize[f]);
	dvi_out(length(font_area[f]));
	dvi_out(length(font_name[f]));
	//@<Output the font name whose internal number is |f|@>;
	for (k=str_start[font_area[f]]; k<=str_start[font_area[f]+1]-1; k++)
	  dvi_out(str_pool[k]);
	for (k=str_start[font_name[f]]; k<=str_start[font_name[f]+1]-1; k++)
	  dvi_out(str_pool[k]);
	//@<Output the font name whose internal number is |f|@>;
}



void load_fonts()
{
	internal_font_number f;
	four_quarters i; //{font information word}
	int j,k,v; //{registers for initializing font tables}
	int m; //:title_font..slant_font+area_code; //{keyword found}
	int n1; //:0..longest_keyword; //{buffered character being checked}
	pool_pointer n2; //{pool character being checked}
	if (interaction) 
		//@<Get online special input@>;
		loop { 
	not_found:
			fprintf(stdout, "\nSpecial font substitution: ");
		
	mycontinue: 
			input_ln();
			if (line_length==0) goto done;
			//@<Search buffer for valid keyword; if successful, |goto found|@>;
			buf_ptr=0; buffer[line_length]=/* */32;
			while (buffer[buf_ptr]!=/* */32) incr(buf_ptr);
			for (m=title_font; m<=slant_font+area_code; m++) 
				if (length(m)==buf_ptr)
				{ 
					n1=0; n2=str_start[m];
					while (n1<buf_ptr && buffer[n1]==str_pool[n2])
					{ 
						incr(n1); incr(n2);
					}
					if (n1==buf_ptr) goto found;
				}
			//@<Search buffer for valid keyword; if successful, |goto found|@>;
			fprintf(stdout, "Please say, e.g., \"grayfont foo\" or \"slantfontarea baz\".");
			goto not_found;
		found: 
			//@<Update the font name or area@>;
			incr(buf_ptr); str_room(line_length-buf_ptr);
			while (buf_ptr<line_length)
			{ 
				append_char(buffer[buf_ptr]); incr(buf_ptr);
			}
			if (m>area_code) font_area[m-area_code]=make_string();
			else { 
				font_name[m]=make_string(); font_area[m]=null_string;
				font_at[m]=0;
			}
			init_str_ptr=str_ptr;
			//@<Update the font name or area@>;

			fprintf(stdout, "OK; any more? ");
			goto mycontinue;
		}
	done:
		//@<Get online special input@>;
	fonts_not_loaded=false;
	for (f=title_font; f <= logo_font; f++)
		if (f != slant_font || length(font_name[f])>0)
		{ 
			if (length(font_area[f])==0) font_area[f]=home_font_area;
			pack_file_name(font_name[f],font_area[f],tfm_ext);
			open_tfm_file(); read_font_info(f,font_at[f]);
			if (font_area[f]==home_font_area) font_area[f]=null_string;
			dvi_font_def(f); //{put the font name in the \.{DVI} file}
		}
	//@<Initialize global variables that depend on the font data@>;
	// 137
	if (length(font_name[slant_font])==0) rule_slant=0.0;
	else { 
		rule_slant=slant(slant_font)/(float)unity;
		slant_n=font_ec[slant_font];
		i=char_info(slant_font,slant_n);
		slant_unit=char_height(slant_font,height_depth(i))/(float)slant_n;
	}
	slant_reported=0.0;
	//169
	i=char_info(gray_font,1);
	if (!char_exists(i)) abort("Missing pixel char!");
	
	unsc_x_ratio=float(char_width(gray_font,i));
	x_ratio=unsc_x_ratio/unity;
	unsc_y_ratio=float(char_height(gray_font,height_depth(i)));
	y_ratio=unsc_y_ratio/unity;
	unsc_slant_ratio=slant(gray_font)*y_ratio;
	slant_ratio=unsc_slant_ratio/unity;
	if (x_ratio*y_ratio==0) abort("Vanishing pixel size!");
	fudge_factor=(slant_ratio/x_ratio)/y_ratio;

	// 175
	gray_rule_thickness=default_rule_thickness(gray_font);
	if (gray_rule_thickness==0) gray_rule_thickness=26214; //{0.4\thinspace pt}

	// 184
	i=char_info(gray_font,0);
	if (!char_exists(i)) abort("Missing dot char!");
	dot_width=char_width(gray_font,i);
	dot_height=char_height(gray_font,height_depth(i));
	delta = space(label_font) / 2;
	thrice_x_height=3*x_height(label_font);
	half_x_height=thrice_x_height / 6;

	// 205
	for (k=0; k<=4095; k++) b[k]=0;
	for (k=font_bc[gray_font]; k<=font_ec[gray_font]; k++)
		if (k>=1) 
			if (k<=120)
				if (char_exists(char_info(gray_font,k)))
				{ 
					v=c[k];
					do {
						b[v]=k; v=v+d[k];
					} while (!(v>4095));
				}

	// 206
	for (j=0; j<=11; j++)
	{ 
		k=two_to_the[j]; v=k;
		do {
			rho[v]=k; v=v+k+k;
		} while (!(v>4095));
	}
	rho[0]=4096;
	//@<Initialize global variables that depend on the font data@>;
}

// 141
node_pointer get_avail()
{
	incr(max_node);
	if (max_node==max_labels) abort("Too many labels and/or rules!");
	return max_node;
}

// 167
void convert(scaled x, scaled y)
{
	x=x+x_offset; y=y+y_offset;
	dvi_y=scaled(-round(y_ratio*y)+delta_y);
	dvi_x=scaled(round(x_ratio*x+slant_ratio*y)+delta_x);
}

// 171
void dvi_goto(scaled x, scaled y)
{
	dvi_out(push);
	if (x != 0)
	{ 
		dvi_out(right4); dvi_four(x);
	}
	if (y!=0)
	{ 
		dvi_out(down4); dvi_four(y);
	}
}

// 145
bool overlap(node_pointer p, node_pointer q)
{
	scaled y_thresh; //{cutoff value to speed the search}
	scaled x_left,x_right,y_top,y_bot; //{boundaries to test for overlap}
	node_pointer r; //{runs through the neighbors of |q|}
	x_left=xl[p]; x_right=xr[p]; y_top=yt[p]; y_bot=yb[p];
	//@<Look for overlaps in the successors of node |q|@>;
	y_thresh=y_bot+max_height; r=next[q];
	while (yy[r]<y_thresh)
	{ 
		if (y_bot > yt[r]) 
			if (x_left<xr[r])
				if (x_right>xl[r]) 
					if (y_top<yb[r])
					{ 
						return true;
					}
		r=next[r];
	}
	//@<Look for overlaps in the successors of node |q|@>;

	//@<Look for overlaps in node |q| and its predecessors@>;
	y_thresh=y_top-max_depth; r=q;
	while (yy[r]>y_thresh)
	{ 
		if (y_bot > yt[r]) 
			if (x_left<xr[r])
				if (x_right>xl[r]) 
					if (y_top<yb[r])
					{ 
						return true;
					}
		r=prev[r];
	}
	//@<Look for overlaps in node |q| and its predecessors@>;
	return false;
}


// 150
node_pointer nearest_dot(node_pointer p, scaled d0)
{
	node_pointer best_q; //{value to return}
	scaled d_min,d; //{distances}
	twin=false; best_q=0; d_min=02000000000;
	//@<Search for the nearest dot in nodes following |p|@>;
	q=next[p];
	while (yy[q]<yy[p]+d_min)
	{ 
		d=myabs(xx[q]-xx[p]);
		if (d<yy[q]-yy[p]) d=yy[q]-yy[p];
		if (d<d0) twin=true;
		else if (d<d_min)
		{ 
			d_min=d; best_q=q;
		}
		q=next[q];
	}
	//@<Search for the nearest dot in nodes following |p|@>;


	//@<Search for the nearest dot in nodes preceding |p|@>;
	q=prev[p];
	while (yy[q]>yy[p]-d_min)
	{ 
		d=myabs(xx[q]-xx[p]);
		if (d<yy[p]-yy[q]) d=yy[p]-yy[q];
		if (d<d0) twin=true;
		else if (d<d_min)
		{ 
			d_min=d; best_q=q;
		}
		q=prev[q];
	}
	//@<Search for the nearest dot in nodes preceding |p|@>;
	return best_q;
}

void do_pixels()
{
	bool paint_black; //{the paint switch}
	int starting_col,finishing_col; //:0..widest_row; //{currently nonzero area}
	int j; //:0..widest_row; //{for traversing that area}
	int l; //{power of two used to manipulate bit patterns}
	four_quarters i; //{character information word}
	eight_bits v; //{character corresponding to a pixel pattern}
	select_font(gray_font);
	delta_x=scaled(delta_x+round(unsc_x_ratio*min_x));
	for (j=0; j<= max_x-min_x; j++) a[j]=0;
	l=1; z=0; starting_col=0; finishing_col=0; y=max_y+12; paint_black=false;
	blank_rows=0; cur_gf=get_byte();
	loop { 
		//@<Add more rows...@>;
		do {
			//@<Put the bits for the next row, times |l|, into |a|@>;
			if (blank_rows>0) decr(blank_rows);
			else if (cur_gf!=eoc)
			{ 
				x=z;
				if (starting_col>x) starting_col=x;
				//@<Read and process \.{GF} commands until coming to the end of this row@>;
				loop {
			mycontinue: 
					switch(cur_gf) {
						case sixty_four_cases(0): k=cur_gf; break;
						case paint1:k=get_byte(); break;
						case paint2:k=get_two_bytes(); break;
						case paint3:k=get_three_bytes(); break;
						case eoc:goto done1; break;
						case skip0:end_with(blank_rows=0; do_skip;); break;
						case skip1:end_with(blank_rows=get_byte(); do_skip;); break;
						case skip2:end_with(blank_rows=get_two_bytes(); do_skip;); break;
						case skip3:end_with(blank_rows=get_three_bytes(); do_skip;); break;
						case sixty_four_cases(new_row_0): case sixty_four_cases(new_row_0+64):
						case thirty_two_cases(new_row_0+128): case five_cases(new_row_0+160):
							end_with(z=cur_gf-new_row_0;paint_black=true);
							break;
						case xxx1: case xxx2: case xxx3: case xxx4: case yyy: case no_op:
							skip_nop(); goto mycontinue;
							break;
						default: bad_gf("Improper opcode"); break;
					}
					//@<Paint |k| bits and read another command@>;
					if (x+k>finishing_col) finishing_col=x+k;
					if (paint_black) 
						for (j=x; j<=x+k-1; j++) a[j]=a[j]+l;
					paint_black=!paint_black;
					x=x+k;
					cur_gf=get_byte();
					//@<Paint |k| bits and read another command@>;
				}
			done1: ;
				//@<Read and process \.{GF} commands until coming to the end of this row@>;
			}
			//@<Put the bits for the next row, times |l|, into |a|@>;
			l=l+l; decr(y);
		} while (!(l==4096));
		//@<Add more rows...@>;
		dvi_goto(0,scaled(delta_y-round(unsc_y_ratio*y)));
		//@<Typeset the pixels...@>;
		j=starting_col;
		loop { 
			while (j<=finishing_col && b[a[j]]==0) incr(j);
			if (j>finishing_col) goto done;
			dvi_out(push); 
			//@<Move to column |j| in the \.{DVI} output@>;
			dvi_out(right4);
			dvi_four(int(round(unsc_x_ratio*j+unsc_slant_ratio*y)+delta_x));

			//@<Move to column |j| in the \.{DVI} output@>;
			do { 
				v=b[a[j]]; a[j]=a[j]-c[v];
				k=j; incr(j);
				while (b[a[j]]==v)
				{ 
					a[j]=a[j]-c[v]; incr(j);
				}
				k=j-k; 
				//@<Output the equivalent of |k| copies of character |v|@>;
			reswitch: 
				if (k==1) typeset(v);
				else { 
					i=char_info(gray_font,v);
					if (char_tag(i)==list_tag) //{|v| has a successor}
					{ 
						if (myodd(k)) typeset(v);
						k=k / 2; v=qo(rem_byte(i)); goto reswitch;
					}
					else do { 
						typeset(v); decr(k);
					} while (!(k==0));
				}
				//@<Output the equivalent of |k| copies of character |v|@>;
			} while (!(b[a[j]]==0));
			dvi_out(pop);
		}
	done:
		//@<Typeset the pixels...@>;
		dvi_out(pop); 
		//@<Advance to the next...@>;
		l=rho[a[starting_col]];
		for (j=starting_col+1; j<=finishing_col; j++) 
			if (l>rho[a[j]]) l=rho[a[j]];
		if (l==4096)
			if (cur_gf==eoc) return;
		else { 
				y=y-blank_rows; blank_rows=0; l=1;
				starting_col=z; finishing_col=z;
			}
		else { 
			while (a[starting_col]==0) incr(starting_col);
			while (a[finishing_col]==0) decr(finishing_col);
			for (j=starting_col; j<=finishing_col; j++) a[j]=a[j] / l;
			l=4096 / l;
		}
		//@<Advance to the next...@>;
	}
}

void do_final_end()
{

	fclose(dvi_file);
	fclose(gf_file);
	printf(" \n");
	exit(0);
}

// 143
void node_ins(node_pointer p, node_pointer q)
{
	node_pointer r; //{for tree traversal}
	if (yy[p]>=yy[q])
	{ 
		do { 
			r=q; q=next[q];
		} while (!(yy[p]<=yy[q]));
		next[r]=p; prev[p]=r; next[p]=q; prev[q]=p;
	}
	else { 
		do {
			r=q; q=prev[q];
		} while (!(yy[p]>=yy[q]));
		prev[r]=p; next[p]=r; prev[p]=q; next[q]=p;
	}
	if (yy[p]-yt[p]>max_height) max_height=yy[p]-yt[p];
	if (yb[p]-yy[p]>max_depth)  max_depth=yb[p]-yy[p];
}

// 185
void top_coords(node_pointer p)
{
	xx[p]=dvi_x-(box_width / 2); xl[p]=xx[p]-delta;
	xr[p]=xx[p]+box_width+delta;
	yb[p]=dvi_y-dot_height; yy[p]=yb[p]-box_depth;
	yt[p]=yy[p]-box_height-delta;
}

// 186
void bot_coords(node_pointer p)
{
	xx[p]=dvi_x-(box_width / 2); xl[p]=xx[p]-delta;
	xr[p]=xx[p]+box_width+delta;
	yt[p]=dvi_y+dot_height; yy[p]=yt[p]+box_height;
	yb[p]=yy[p]+box_depth+delta;
}

void right_coords(node_pointer p)
{
	xl[p]=dvi_x+dot_width; xx[p]=xl[p]; xr[p]=xx[p]+box_width+delta;
	yy[p]=dvi_y+half_x_height; yb[p]=yy[p]+box_depth+delta;
	yt[p]=yy[p]-box_height-delta;
}

void left_coords(node_pointer p)
{
	xr[p]=dvi_x-dot_width; xx[p]=xr[p]-box_width; xl[p]=xx[p]-delta;
	yy[p]=dvi_y+half_x_height; yb[p]=yy[p]+box_depth+delta;
	yt[p]=yy[p]-box_height-delta;
}

// 194
bool place_label(node_pointer p)
{
	int oct; //:0..15; {octant code}
	node_pointer dfl; //{saved value of |dot_for_label[p]|}
	hbox(info[p],label_font,false); //{Compute the size of this label}
	dvi_x=xx[p]; dvi_y=yy[p];
	//@<Find non-overlapping coordinates, if possible, and |goto| found; otherwise set |place_label:=false| and |return|@>;
	dfl=dot_for_label[p]; oct=octant[p];
	//@<Try the first choice for label direction@>;
	switch(oct) {
		case first_octant: case eighth_octant: case second_octant+8: case seventh_octant+8: left_coords(p); break;
		case second_octant: case third_octant: case first_octant+8: case fourth_octant+8: bot_coords(p); break;
		case fourth_octant: case fifth_octant: case third_octant+8: case sixth_octant+8: right_coords(p); break;
		case sixth_octant: case seventh_octant: case fifth_octant+8: case eighth_octant+8: top_coords(p); break;
	}
	if (!overlap(p,dfl)) goto found;
	//@<Try the first choice for label direction@>;

	//@<Try the second choice for label direction@>;
	switch(oct) {
		case first_octant: case fourth_octant: case fifth_octant+8: case eighth_octant+8: bot_coords(p); break;
		case second_octant: case seventh_octant: case third_octant+8: case sixth_octant+8: left_coords(p); break;
		case third_octant: case sixth_octant: case second_octant+8: case seventh_octant+8: right_coords(p); break;
		case fifth_octant: case eighth_octant: case first_octant+8: case fourth_octant+8: top_coords(p); break;
	}
	if (!overlap(p,dfl)) goto found;
	//@<Try the second choice for label direction@>;

	//@<Try the third choice for label direction@>;
	switch(oct) {
		case first_octant: case fourth_octant: case sixth_octant+8: case seventh_octant+8: top_coords(p); break;
		case second_octant: case seventh_octant: case fourth_octant+8: case fifth_octant+8: right_coords(p); break;
		case third_octant: case sixth_octant: case first_octant+8: case eighth_octant+8: left_coords(p); break;
		case fifth_octant: case eighth_octant: case second_octant+8: case third_octant+8: bot_coords(p); break;
	}
	if (!overlap(p,dfl)) goto found;
	//@<Try the third choice for label direction@>;

	//@<Try the fourth choice for label direction@>;
	switch(oct) {
		case first_octant: case eighth_octant: case first_octant+8: case eighth_octant+8: right_coords(p); break;
		case second_octant: case third_octant: case second_octant+8: case third_octant+8: top_coords(p); break;
		case fourth_octant: case fifth_octant: case fourth_octant+8: case fifth_octant+8: left_coords(p); break;
		case sixth_octant: case seventh_octant: case sixth_octant+8: case seventh_octant+8: bot_coords(p); break;
	}
	if (!overlap(p,dfl)) goto found;
	//@<Try the fourth choice for label direction@>;
	xx[p]=dvi_x;  yy[p]=dvi_y; dot_for_label[p]=dfl; //{no luck; restore the coordinates}
	return false;
	//@<Find non-overlapping coordinates, if possible, and |goto| found; otherwise set |place_label:=false| and |return|@>;

found:
	node_ins(p,dfl);
	dvi_goto(xx[p],yy[p]); hbox(info[p],label_font,true); dvi_out(pop);
	return true;
}


int main(int argc, char *argv[])
{
	initialize();


	
	//@<Initialize the strings@>=
	// 77
	str_ptr=0; pool_ptr=0; str_start[0]=0;
	l=0; init_str0(null_string);
	l=9; init_str9(/*t*/116,/*i*/105,/*t*/116,/*l*/108,/*e*/101,/*f*/102,/*o*/111,/*n*/110,/*t*/116,title_font);
	l=9; init_str9(/*l*/108,/*a*/97,/*b*/98,/*e*/101,/*l*/108,/*f*/102,/*o*/111,/*n*/110,/*t*/116,label_font);
	l=8; init_str8(/*g*/103,/*r*/114,/*a*/97,/*y*/121,/*f*/102,/*o*/111,/*n*/110,/*t*/116,gray_font);
	l=9; init_str9(/*s*/115,/*l*/108,/*a*/97,/*n*/110,/*t*/116,/*f*/102,/*o*/111,/*n*/110,/*t*/116,slant_font);
	l=13; init_str13(/*t*/116,/*i*/105,/*t*/116,/*l*/108,/*e*/101,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*r*/114,/*e*/101,/*a*/97,title_font+area_code);
	l=13; init_str13(/*l*/108,/*a*/97,/*b*/98,/*e*/101,/*l*/108,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*r*/114,/*e*/101,/*a*/97,label_font+area_code);
	l=12; init_str12(/*g*/103,/*r*/114,/*a*/97,/*y*/121,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*r*/114,/*e*/101,/*a*/97,gray_font+area_code);
	l=13; init_str13(/*s*/115,/*l*/108,/*a*/97,/*n*/110,/*t*/116,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*r*/114,/*e*/101,/*a*/97,slant_font+area_code);
	l=11; init_str11(/*t*/116,/*i*/105,/*t*/116,/*l*/108,/*e*/101,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*t*/116,title_font+at_code);
	l=11; init_str11(/*l*/108,/*a*/97,/*b*/98,/*e*/101,/*l*/108,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*t*/116,label_font+at_code);
	l=10; init_str10(/*g*/103,/*r*/114,/*a*/97,/*y*/121,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*t*/116,gray_font+at_code);
	l=11; init_str11(/*s*/115,/*l*/108,/*a*/97,/*n*/110,/*t*/116,/*f*/102,/*o*/111,/*n*/110,/*t*/116,/*a*/97,/*t*/116,slant_font+at_code);
	l=4; init_str4(/*r*/114,/*u*/117,/*l*/108,/*e*/101,rule_code);
	l=5; init_str5(/*t*/116,/*i*/105,/*t*/116,/*l*/108,/*e*/101,title_code);
	l=13; init_str13(/*r*/114,/*u*/117,/*l*/108,/*e*/101,/*t*/116,/*h*/104,/*i*/105,/*c*/99,/*k*/107,/*n*/110,/*e*/101,/*s*/115,/*s*/115,rule_thickness_code);
	l=6; init_str6(/*o*/111,/*f*/102,/*f*/102,/*s*/115,/*e*/101,/*t*/116,offset_code);
	l=7; init_str7(/*x*/120,/*o*/111,/*f*/102,/*f*/102,/*s*/115,/*e*/101,/*t*/116,x_offset_code);
	l=7; init_str7(/*y*/121,/*o*/111,/*f*/102,/*f*/102,/*s*/115,/*e*/101,/*t*/116,y_offset_code);

	// 78
	l=7; init_str7(/*.*/46,/*2*/50,/*6*/54,/*0*/48,/*2*/50,/*g*/103,/*f*/102,gf_ext);
	l=4; init_str4(/*.*/46,/*d*/100,/*v*/118,/*i*/105,dvi_ext);
	l=4; init_str4(/*.*/46,/*t*/116,/*f*/102,/*m*/109,tfm_ext);
	l=7; init_str7(/* */32,/* */32,/*P*/80,/*a*/97,/*g*/103,/*e*/101,/* */32,page_header);
	l=12; init_str12(/* */32,/* */32,/*C*/67,/*h*/104,/*a*/97,/*r*/114,/*a*/97,/*c*/99,/*t*/116,/*e*/101,/*r*/114,/* */32,char_header);
	l=6; init_str6(/* */32,/* */32,/*E*/69,/*x*/120,/*t*/116,/* */32,ext_header);
	l=4; init_str4(/* */32,/* */32,/*`*/96,/*`*/96,left_quotes);
	l=2; init_str2(/*'*/39,/*'*/39,right_quotes);
	l=3; init_str3(/* */32,/*=*/61,/* */32,equals_sign);
	l=4; init_str4(/* */32,/*+*/43,/* */32,/*(*/40,plus_sign);
	l=4; init_str4(/*c*/99,/*m*/109,/*r*/114,/*8*/56,default_title_font);
	l=6; init_str6(/*c*/99,/*m*/109,/*t*/116,/*t*/116,/*1*/49,/*0*/48,default_label_font);
	l=4; init_str4(/*g*/103,/*r*/114,/*a*/97,/*y*/121,default_gray_font);
	l=5; init_str5(/*l*/108,/*o*/111,/*g*/103,/*o*/111,/*8*/56,logo_font_name);
	l=8; init_str8(/*M*/77,/*E*/69,/*T*/84,/*A*/65,/*F*/70,/*O*/79,/*N*/78,/*T*/84,small_logo);

	// 88	
	l=0; init_str0(home_font_area);


	//@<Initialize the strings@>=

	set_paths(); // {Initialize paths for TFM files from environment if needed}

	start_gf(argc, argv); // open the input and output files

	//@<Process the preamble@>=
	if (get_byte() != pre) bad_gf("No preamble");
	if (get_byte() != gf_id_byte) bad_gf("Wrong ID");
	k=get_byte(); //{|k| is the length of the initial string to be copied}
	for (m=1; m <= k; m++) append_char(get_byte());
	dvi_out(pre); dvi_out(dvi_id_byte); //{output the preamble}
	dvi_four(25400000); dvi_four(473628672); //{conversion ratio for sp}
	dvi_four(1000); //{magnification factor}
	dvi_out(k); use_logo=false; s=str_start[str_ptr];
	for (m=1; m<= k; m++) dvi_out(str_pool[s+m-1]);
	if (str_pool[s] == /* */32)
		if (str_pool[s+1] == /*M*/77)
			if (str_pool[s+2]==/*E*/69)
				if (str_pool[s+3]==/*T*/84)
					if (str_pool[s+4]==/*A*/65)
						if (str_pool[s+5]==/*F*/70)
							if (str_pool[s+6]==/*O*/79)
								if (str_pool[s+7]==/*N*/78)
									if (str_pool[s+8]==/*T*/84)
									{ 
										incr(str_ptr); str_start[str_ptr]=s+9; use_logo=true;
									} //{we will substitute `\MF' for \.{METAFONT}}
	time_stamp=make_string();
	//@<Process the preamble@>=

	cur_gf = get_byte(); init_str_ptr = str_ptr;

	loop { 
		//@<Initialize variables for the next character@>;
		max_node=0; next[0]=end_of_list; prev[end_of_list]=0;
		max_height=0; max_depth=0;

		rule_thickness=0;
		offset_x=0; offset_y=0; x_offset=0; y_offset=0;
		pre_min_x=02000000000; pre_max_x=-02000000000;
		pre_min_y=02000000000; pre_max_y=-02000000000;

		rule_ptr=null;
		title_head=null; title_tail=null; label_head=null; label_tail=end_of_list;
		first_dot=max_labels;

		//@<Initialize variables for the next character@>;
		while (cur_gf >= xxx1 && cur_gf <= no_op) 
			//@<Process a no-op command@>;
		{ 
			k=interpret_xxx();
			switch(k) {
				case no_operation: do_nothing; break;
				case title_font: case label_font: case gray_font: case slant_font:
					font_change(font_name[k]=cur_string;font_area[k]=null_string;font_at[k]=0;init_str_ptr=str_ptr);
					break;
				case title_font+area_code: case label_font+area_code: case gray_font+area_code:
				case slant_font+area_code:
					font_change(font_area[k-area_code]=cur_string;init_str_ptr=str_ptr);
					break;
				case title_font+at_code: case label_font+at_code: case gray_font+at_code:
				case slant_font+at_code: 
					font_change(font_at[k-at_code]=get_yyy();init_str_ptr=str_ptr);
					break;
				case rule_thickness_code:
					rule_thickness=get_yyy();
					break;
				case rule_code:
					//@<Store a rule@>;
					p=get_avail(); next[p]=rule_ptr; rule_ptr=p;
					x0[p]=get_yyy(); y0[p]=get_yyy(); x1[p]=get_yyy(); y1[p]=get_yyy();
					if (x0[p]<pre_min_x) pre_min_x=x0[p];
					if (x0[p]>pre_max_x) pre_max_x=x0[p];
					if (y0[p]<pre_min_y) pre_min_y=y0[p];
					if (y0[p]>pre_max_y) pre_max_y=y0[p];
					if (x1[p]<pre_min_x) pre_min_x=x1[p];
					if (x1[p]>pre_max_x) pre_max_x=x1[p];
					if (y1[p]<pre_min_y) pre_min_y=y1[p];
					if (y1[p]>pre_max_y) pre_max_y=y1[p];
					rule_size[p]=rule_thickness;
					//@<Store a rule@>;
					break;
				case offset_code:
					//@<Override the offsets@>;
					offset_x=get_yyy(); offset_y=get_yyy();
					//@<Override the offsets@>;
					break;
				case x_offset_code:
					x_offset=get_yyy();
					break;
				case y_offset_code:
					y_offset=get_yyy();
					break;
				case title_code:
					//@<Store a title@>;
					p=get_avail(); info[p]=cur_string;
					if (title_head==null) title_head=p;
					else next[title_tail]=p;
					title_tail=p;
					//@<Store a title@>;
					break;
				case null_string:
					//@<Store a label@>;
					if (label_type < /*/*/47 || label_type > /*8*/56)
						fprintf(stdout, "\nBad label type precedes byte %d!", cur_loc);
					else { 
						p=get_avail(); next[label_tail]=p; label_tail=p;
						lab_typ[p]=label_type; info[p]=cur_string;
						xx[p]=get_yyy(); yy[p]=get_yyy();
						if (xx[p]<pre_min_x) pre_min_x=xx[p];
						if (xx[p]>pre_max_x) pre_max_x=xx[p];
						if (yy[p]<pre_min_y) pre_min_y=yy[p];
						if (yy[p]>pre_max_y) pre_max_y=yy[p];
					}
					//@<Store a label@>;
					break;
			} //{there are no other cases}
		}
			//@<Process a no-op command@>;
		if (cur_gf==post) 
			//@<Finish the \.{DVI} file and |goto final_end|@>;
		{ 
			dvi_out(post); //{beginning of the postamble}
			dvi_four(last_bop); last_bop=dvi_offset+dvi_ptr-5; //{|post| location}
			dvi_four(25400000); dvi_four(473628672); //{conversion ratio for sp}
			dvi_four(1000); //{magnification factor}
			dvi_four(max_v); dvi_four(max_h);
			dvi_out(0); dvi_out(3); //{`\\{max\_push}' is said to be 3}@/
			dvi_out(total_pages / 256); dvi_out(total_pages % 256);
			if (!fonts_not_loaded)
				for (k=title_font; k<= logo_font; k++)
					if (length(font_name[k]) > 0) dvi_font_def(k);
			dvi_out(post_post); dvi_four(last_bop); dvi_out(dvi_id_byte);
			k=4+((dvi_buf_size-dvi_ptr) % 4); //{the number of 223's}
			while (k>0)
			{ 
				dvi_out(223); decr(k);
			}
			//@<Empty the last bytes out of |dvi_buf|@>;
			if (dvi_limit==half_buf) write_dvi(half_buf,dvi_buf_size-1);
			if (dvi_ptr>0) write_dvi(0,dvi_ptr-1);

			//@<Empty the last bytes out of |dvi_buf|@>;
			//goto final_end;
			do_final_end();
		}



			//@<Finish the \.{DVI} file and |goto final_end|@>;
		if (cur_gf != boc) 
			if (cur_gf != boc1) abort("Missing boc!");
		//@<Process a character@>;
		check_fonts;
		//@<Finish reading the parameters of the |boc|@>;
		if (cur_gf==boc)
		{ 
			ext=signed_quad(); //{read the character code}
			char_code=ext % 256;
			if (char_code<0) char_code=char_code+256;
			ext=(ext-char_code) / 256;
			k=signed_quad(); //{read and ignore the prev pointer}
			min_x=signed_quad(); //{read the minimum $x$ coordinate}
			max_x=signed_quad(); //{read the maximum $x$ coordinate}
			min_y=signed_quad(); //{read the minimum $y$ coordinate}
			max_y=signed_quad(); //{read the maximum $y$ coordinate}
		}
		else { 
			ext=0; char_code=get_byte(); //{|cur_gf=boc1|}
			min_x=get_byte(); max_x=get_byte(); min_x=max_x-min_x;
			min_y=get_byte(); max_y=get_byte(); min_y=max_y-min_y;
		}
		if (max_x-min_x>widest_row) abort("Character too wide!");
		//@<Finish reading the parameters of the |boc|@>;

		//@<Get ready to convert \MF\ coordinates to \.{DVI} coordinates@>;
		if (pre_min_x < min_x*unity) offset_x=offset_x+min_x*unity-pre_min_x;
		if (pre_max_y>max_y*unity) offset_y=offset_y+max_y*unity-pre_max_y;
		if (pre_max_x>max_x*unity) pre_max_x=pre_max_x / unity;
		else pre_max_x=max_x;
		if (pre_min_y<min_y*unity) pre_min_y=pre_min_y / unity;
		else pre_min_y=min_y;
		delta_y=scaled(round(unsc_y_ratio*(max_y+1)-y_ratio*offset_y)+3276800);
		delta_x=scaled(round(x_ratio*offset_x-unsc_x_ratio*min_x));
		if (slant_ratio>=0)
			over_col=scaled(round(unsc_x_ratio*pre_max_x+unsc_slant_ratio*max_y));
		else over_col=scaled(round(unsc_x_ratio*pre_max_x+unsc_slant_ratio*min_y));
		over_col=over_col+delta_x+10000000;
		page_height=scaled(round(unsc_y_ratio*(max_y+1-pre_min_y))+3276800-offset_y);
		if (page_height>max_v) max_v=page_height;
		page_width=over_col-10000000;
		//@<Get ready to convert \MF\ coordinates to \.{DVI} coordinates@>;


		//@<Output the |bop| and the title line@>;
		dvi_out(bop); incr(total_pages); dvi_four(total_pages);
		dvi_four(char_code); dvi_four(ext);
		for (k=3; k<=9; k++) dvi_four(0);
		dvi_four(last_bop); last_bop=dvi_offset+dvi_ptr-45;
		dvi_goto(0,655360); //{the top baseline is 10\thinspace pt down}
		if (use_logo)
		{ 
			select_font(logo_font); hbox(small_logo,logo_font,true);
		}
		select_font(title_font); hbox(time_stamp,title_font,true);
		hbox(page_header,title_font,true); dvi_scaled(total_pages*65536.0f);
		if (char_code!=0 || ext!=0)
		{
			hbox(char_header,title_font,true); dvi_scaled(char_code*65536.0f);
			if (ext!=0)
			{ 
				hbox(ext_header,title_font,true); dvi_scaled(ext*65536.0f);
			}
		}
		if (title_head!=null)
		{ 
			next[title_tail]=null;
			do { 
				hbox(left_quotes,title_font,true);
				hbox(info[title_head],title_font,true);
				hbox(right_quotes,title_font,true);
				title_head=next[title_head];
			} while (!(title_head==null));
		}
		dvi_out(pop);
		//@<Output the |bop| and the title line@>;

		fprintf(stdout, "[%d", total_pages);
		update_terminal(); //{print a progress report}
		//@<Output all rules for the current character@>;
		if (rule_slant != 0) select_font(slant_font);
		while (rule_ptr!=null)
		{ 
			p=rule_ptr; rule_ptr=next[p];
			if (rule_size[p]==0) rule_size[p]=gray_rule_thickness;
			if (rule_size[p]>0)
			{ 
				convert(x0[p],y0[p]); temp_x=dvi_x; temp_y=dvi_y;
				convert(x1[p],y1[p]);
				if (myabs(temp_x-dvi_x)<tol)
					//@<Output a vertical rule@>
				{ 
					if (temp_y>dvi_y)
					{
						k=temp_y; temp_y=dvi_y; dvi_y=k;
					}
					dvi_goto(dvi_x-(rule_size[p] / 2), dvi_y);
					dvi_out(put_rule); dvi_four(dvi_y-temp_y); dvi_four(rule_size[p]);
					dvi_out(pop);
				}
					//@<Output a vertical rule@>
				else if (myabs(temp_y-dvi_y)<tol)
					//@<Output a horizontal rule@>
				{ 
					if (temp_x<dvi_x)
					{ 
						k=temp_x; temp_x=dvi_x; dvi_x=k;
					}
					dvi_goto(dvi_x,dvi_y+(rule_size[p] / 2));
					dvi_out(put_rule); dvi_four(rule_size[p]); dvi_four(temp_x-dvi_x);
					dvi_out(pop);
				}
					//@<Output a horizontal rule@>
				else 
					//@<Try to output a diagonal rule@>;
					if (rule_slant==0 || fabs(temp_x+rule_slant*(temp_y-dvi_y)-dvi_x)>rule_size[p])
						slant_complaint((float)(dvi_x-temp_x)/(temp_y-dvi_y));
					else { 
						if (temp_y>dvi_y)
						{ 
							k=temp_y; temp_y=dvi_y; dvi_y=k;
							k=temp_x; temp_x=dvi_x; dvi_x=k;
						}
						m=int(round((dvi_y-temp_y)/(float)slant_unit));
						if (m>0)
						{ 
							dvi_goto(dvi_x,dvi_y);
							q=((m-1) / slant_n)+1; k=m / q;
							p=m % q; q=q-p;
							//@<Vertically typeset |q| copies of character |k|@>;
							typeset(k); dy=int(round(k*slant_unit)); dvi_out(z4); dvi_four(-dy);
							while (q>1)
							{ 
								typeset(k); dvi_out(z0); decr(q);
							}
							//@<Vertically typeset |q| copies of character |k|@>;


							//@<Vertically typeset |p| copies of character |k+1|@>;
							if (p>0)
							{ 
								incr(k); typeset(k);
								dy=int(round(k*slant_unit)); dvi_out(z4); dvi_four(-dy);
								while (p>1)
								{ 
									typeset(k); dvi_out(z0); decr(p);
								}
							}
							//@<Vertically typeset |p| copies of character |k+1|@>;
							dvi_out(pop);
						}
					}
					//@<Try to output a diagonal rule@>;
			}
		}
		//@<Output all rules for the current character@>;


		//@<Output all labels for the current character@>;
		overflow_line=1;
		if (label_head!=null)
		{ 
			next[label_tail]=null; select_font(gray_font);
			//@<Output all dots@>;
			p=label_head; first_dot=max_node+1;
			while (p != null)
			{ 
				convert(xx[p],yy[p]); xx[p]=dvi_x; yy[p]=dvi_y;
				if (lab_typ[p] < /*5*/53)
					//@<Enter a dot for label |p| in the rectangle list, and typeset the dot@>;
				{ 
					q=get_avail(); dot_for_label[p]=q; label_for_dot[q]=p;
					xx[q]=dvi_x; xl[q]=dvi_x-dot_width; xr[q]=dvi_x+dot_width;
					yy[q]=dvi_y; yt[q]=dvi_y-dot_height; yb[q]=dvi_y+dot_height;
					node_ins(q,0);
					dvi_goto(xx[q],yy[q]); dvi_out(0); dvi_out(pop);
				}
					//@<Enter a dot for label |p| in the rectangle list, and typeset the dot@>;
				p=next[p];
			}
			//@<Output all dots@>;


			//@<Find nearest dots, to help in label positioning@>;
			p=label_head;
			while (p!=null)
			{ 
				if (lab_typ[p]<=/*0*/48)
					//@<Compute the octant code for floating label |p|@>;
				{ 
					r=dot_for_label[p]; q=nearest_dot(r,10);
					if (twin) octant[p]=8; else octant[p]=0;
					if (q!=null)
					{ 
						dx=xx[q]-xx[r]; dy=yy[q]-yy[r];
						if (dy>0) octant[p]=octant[p]+4;
						if (dx<0) incr(octant[p]);
						if (dy>dx) incr(octant[p]);
						if (-dy>dx) incr(octant[p]);
					}
				}
					//@<Compute the octant code for floating label |p|@>;
				p=next[p];
			}
			//@<Find nearest dots, to help in label positioning@>;
			select_font(label_font);
			//@<Output all prescribed labels@>;
			q=end_of_list; //{|label_head=next[q]|}
			while (next[q]!=null)
			{ 
				p=next[q];
				if (lab_typ[p]>/*0*/48)
				{ 
					next[q]=next[p];
					//@<Enter a prescribed label for node |p| into the rectangle list, and typeset it@>;
					hbox(info[p],label_font,false); //{Compute the size of this label}
					dvi_x=xx[p];  dvi_y=yy[p];
					if (lab_typ[p]</*5*/53) r=dot_for_label[p]; else r=0;
					switch(lab_typ[p]) {
						case /*1*/49: case /*5*/53:top_coords(p); break;
						case /*2*/50: case /*6*/54:left_coords(p); break;
						case /*3*/51: case /*7*/55:right_coords(p); break;
						case /*4*/52: case /*8*/56:bot_coords(p); break;
					} //{no other cases are possible}
					node_ins(p,r);
					dvi_goto(xx[p],yy[p]); hbox(info[p],label_font,true); dvi_out(pop);
					//@<Enter a prescribed label for node |p| into the rectangle list, and typeset it@>;
				}
				else q=next[q];
			}
			//@<Output all prescribed labels@>;

			//@<Output all attachable labels@>;
			q=end_of_list; //{now |next[q]=label_head|}
			while (next[q]!=null)
			{ 
				p=next[q]; r=next[p]; s=dot_for_label[p];
				if (place_label(p)) next[q]=r;
				else { 
					label_for_dot[s]=null; //{disconnect the dot}
					if (lab_typ[p]==/*/*/47) next[q]=r; //{remove label from list}
					else q=p; //{retain label in list for the overflow column}
				}
			}
			//@<Output all attachable labels@>;
			
			//@<Output all overflow labels@>;

			//@<Remove all rectangles from list, except for dots that have labels@>;
			p=next[0];
			while (p!=end_of_list)
			{ 
				q=next[p];
				if (p<first_dot || label_for_dot[p]==null)
				{ 
					r=prev[p]; next[r]=q; prev[q]=r; next[p]=r;
				}
				p=q;
			}
			//@<Remove all rectangles from list, except for dots that have labels@>;
			p=label_head;
			while (p!=null)
			{ 
				//@<Typeset an overflow label for |p|@>;
				r=next[dot_for_label[p]]; s=next[r]; t=next[p];
				next[p]=s; prev[s]=p; next[r]=p; prev[p]=r;
				q=nearest_dot(p,0);
				next[r]=s; prev[s]=r; next[p]=t; //{remove |p| again}
				incr(overflow_line);
				dvi_goto(over_col,overflow_line*thrice_x_height+655360);
				hbox(info[p],label_font,true);
				if (q!=null)
				{ 
					hbox(equals_sign,label_font,true);
					hbox(info[label_for_dot[q]],label_font,true);
					hbox(plus_sign,label_font,true);
					dvi_scaled((xx[p]-xx[q])/x_ratio+(yy[p]-yy[q])*fudge_factor);
					dvi_out(/*,*/44);
					dvi_scaled((yy[q]-yy[p])/y_ratio);
					dvi_out(/*)*/41);
				}
				dvi_out(pop);
				//@<Typeset an overflow label for |p|@>;
				p=next[p];
			}
			//@<Output all overflow labels@>;
		}
		//@<Output all labels for the current character@>;


		do_pixels();
		dvi_out(eop); //{finish the page}
		//@<Adjust the maximum page width@>;
		if (overflow_line>1) page_width=over_col+10000000;
			//{overflow labels are estimated to occupy $10^7\,$sp}
		if (page_width>max_h) max_h=page_width;
		//@<Adjust the maximum page width@>;

		fprintf(stdout, "]");
		update_terminal();
		//@<Process a character@>;
		cur_gf=get_byte(); str_ptr=init_str_ptr; pool_ptr=str_start[str_ptr];
	}
//final_end:
	do_final_end();
}

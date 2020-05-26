#include <windows.h>
#include <stdio.h>
#include <wincon.h>
#include <stdlib.h>

#define MAIN_WINDOW_HEIGHT 40
#define MAIN_WINDOW_WIDTH 99 
#define WINDOWS_NUMBER 3 
#define WINDOW_WIDTH MAIN_WINDOW_WIDTH / WINDOWS_NUMBER 
#define WINDOW_HEIGHT 5
#define NUMBER_OF_VISITERS 24 
#define TERMINAL_WORKING_TIME 3 * 1000 
#define X_POS_FOR_TERMINAL (MAIN_WINDOW_WIDTH/2 - 10) 
#define Y_POS_FOR_TERMINAL (MAIN_WINDOW_HEIGHT/2 - 2)
#define TIME_OF_SLEEP 100 

typedef struct 
{
	char id;
	int time;
	int x;
	int y;
	int in_building;
} ThreadArgs;

typedef struct { 
	int id;
	int time;
	char visiter;
} WindowInfo;

typedef struct { 
	int last_ticket_id;
	char visiter;
} TicketInfo;

void cls_scr(); 
void write_many(char, int, int, int); 
void make_window(int, int, int, int); 
void refresh_window_info(int); 
void write(char *, int, int); 
void setCursor(int, int); 
void start_model(); 
void show_model(); 
void change_window_info(int, int, int, char); 
int change_ticket_info(char); 
void refresh_ticket_window_info(); 
void refresh_model(); 
void get_to(ThreadArgs *, int, int); 
void change_position(ThreadArgs *, int, int); 
int check(int, int); 
DWORD WINAPI ThreadVisiter(LPVOID); 

WindowInfo WindInfo[WINDOWS_NUMBER]; 
TicketInfo TickInfo;

ThreadArgs * Params[NUMBER_OF_VISITERS]; 
HANDLE ThreadVisiters[NUMBER_OF_VISITERS]; 
HANDLE WriteMutex; 
HANDLE TerminalMutex; 
HANDLE WindowMutexes[WINDOWS_NUMBER]; 

int main() 
{
	for(int i = 0;i < NUMBER_OF_VISITERS;i++) 
	{
		ThreadArgs * test = (ThreadArgs *)malloc(sizeof(ThreadArgs)); 
		test->id = i + 'A'; 
		test->time = (rand() % 270 + 30) * 350; 
		test->x = MAIN_WINDOW_WIDTH/4 + i*2; 
		test->y = MAIN_WINDOW_HEIGHT - 3;
		test->in_building = 0;
		Params[i] = test;
	}

	start_model();
	show_model();

	WriteMutex = CreateMutex(NULL, 0, NULL); 

	if(WriteMutex == NULL) 
	{
		printf("Failed to make a Write mutex");
		return 1;
	}

	TerminalMutex = CreateMutex(NULL, 0, NULL);

	if(TerminalMutex == NULL) 
	{
		printf("Failed to make a Terminal mutex");
		return 1;
	}

	for(int i = 0;i < WINDOWS_NUMBER;i++)
	{
		WindowMutexes[i] = CreateMutex(NULL, 0, NULL);

		if(WindowMutexes[i] == NULL)
		{
			printf("Failed to make a terminal mutex");
			return 1;
		}
	}

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) 
	{
		int * id = malloc(sizeof(int));
		*id = i;
		ThreadVisiters[i] = CreateThread(NULL, 0, ThreadVisiter, id, 0, 0); 

		if(ThreadVisiters[i] == NULL)
		{
			printf("Failed to make a Thread");
			return 1;
		}
	}

	WaitForMultipleObjects(NUMBER_OF_VISITERS, ThreadVisiters, 1, INFINITE); 
	setCursor(0,0); 

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) 
	{
		CloseHandle(ThreadVisiters[i]); 
	}

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) 
	{
		free(Params[i]);
	}

	for(int i = 0;i < WINDOWS_NUMBER;i++) 
	{
		CloseHandle(WindowMutexes[i]);
	}

	CloseHandle(WriteMutex);
	CloseHandle(TerminalMutex);

	getchar(); 
	cls_scr(); 

	return 0;
}

int get_length(char * string) 
{
	int i = 0;
	while(*(string + i) != '\0')
	{
		i++;
	}

	return i;
}

void setCursor(int x, int y)
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE); 

	COORD pos;
	pos.X = x;
	pos.Y = y;

	SetConsoleCursorPosition(hout, pos);
}

void write(char * string, int x, int y)
{
	int len = get_length(string);
	DWORD written;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

	setCursor(x , y);

	WriteConsole(hout, string, len, &written, NULL);
}

void write_many(char symbol, int x, int y, int num)
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos;
	DWORD written;

	pos.X = x;
	pos.Y = y;
	
	FillConsoleOutputCharacter(hout, symbol, (WORD) num, pos, &written);
}

void make_window(int x, int y, int width, int height)
{
	write_many('-',x,y,width);
	write_many('-',x,y+height,width);

	for(int i = y+1;i < y+height;i++)
	{
		write("|", x, i);
		write("|", x+width-1, i);
	}
}

void start_model()
{
	CONSOLE_CURSOR_INFO * info = malloc(sizeof(CONSOLE_CURSOR_INFO)); 
	info->dwSize = 1;
	info->bVisible = 0;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleCursorInfo(hout, info);
	free(info);

	for(int i = 0; i < WINDOWS_NUMBER; i++) 
	{
		change_window_info(i, 0, 0, 0);
	}

	TickInfo.last_ticket_id = 0; 
	TickInfo.visiter = 0;

}

void refresh_ticket_window_info()
{
	char* ticket_string = (char *) malloc(sizeof(char) * 30);
	char* visiter_string = (char *) malloc(sizeof(char) * 30);

	sprintf(ticket_string, "Last ticket id %d", TickInfo.last_ticket_id);
	sprintf(visiter_string, "Visiter id %c", TickInfo.visiter);

	write(ticket_string, X_POS_FOR_TERMINAL+1 , Y_POS_FOR_TERMINAL+1);
	write(visiter_string, X_POS_FOR_TERMINAL+1 , Y_POS_FOR_TERMINAL+2);

	free(ticket_string);
	free(visiter_string);
}

void refresh_window_info(int window_id)
{
	WindowInfo * info = &WindInfo[window_id];
	char* ticket_string = (char *) malloc(sizeof(char) * 30);
	char* visiter_string = (char *) malloc(sizeof(char) * 30);
	char* time_string = (char *) malloc(sizeof(char) * 30);

	sprintf(ticket_string, "Ticket id - %d", info->id);
	sprintf(time_string, "Time - %dc", info->time);
	sprintf(visiter_string, "Visiter - %c", info->visiter);

	write(ticket_string, 3 + window_id*WINDOW_WIDTH, 3);
	write(time_string, 3 + window_id*WINDOW_WIDTH, 4);
	write(visiter_string, 3 + window_id*WINDOW_WIDTH, 5);

	free(ticket_string);
	free(time_string);
	free(visiter_string);
}

void change_window_info(int window_id, int ticket_id, int time, char visiter)
{
	WindowInfo * info = &WindInfo[window_id]; 
	info->id = ticket_id;
	info->time = time;
	info->visiter = visiter;
}

int change_ticket_info(char visiter)
{
	TickInfo.visiter = visiter;
	int ticket_id = ++TickInfo.last_ticket_id;
	return ticket_id;
}

void show_model()
{
	cls_scr();
	make_window(2, 2, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT); 
	
	for(int i = 0; i < WINDOWS_NUMBER; i++) 
	{
		make_window(2 + i*WINDOW_WIDTH, 2, WINDOW_WIDTH, WINDOW_HEIGHT);
		refresh_window_info(i);
	}

	make_window(X_POS_FOR_TERMINAL, Y_POS_FOR_TERMINAL, 20, 5); 
	refresh_ticket_window_info();
	
	setCursor(0, 0);
}

void refresh_model()
{
	for(int i = 0; i < WINDOWS_NUMBER; i++)
	{
		refresh_window_info(i);
	}

	refresh_ticket_window_info();

	for(int i = 0; i < NUMBER_OF_VISITERS; i++) 
	{
		if(Params[i]->in_building)
		{
			char * id = malloc(sizeof(char));
			sprintf(id, "%c", Params[i]->id);

			write(id, Params[i]->x, Params[i]->y);
			free(id);
		}
	}
}

int check(int x, int y)
{
	for(int i = 0; i < NUMBER_OF_VISITERS; i++)
	{
		if(Params[i]->in_building && Params[i]->x == x && Params[i]->y == y)
		{
			return 0;
		}
	}

	return 1;
}

void change_position(ThreadArgs * params, int x, int y)
{
	if(check(x, y)) {
		WaitForSingleObject(WriteMutex, INFINITE);

		write(" ", params->x, params->y);
		params->x = x;
		params->y = y;
		refresh_model();

		ReleaseMutex(WriteMutex);
	}
}

void get_to(ThreadArgs * params, int x, int y)
{
	while(1)
	{
		Sleep(TIME_OF_SLEEP);

		if(x != params->x || y != params->y)
		{	
			if(x < params->x) 
			{
				change_position(params, params->x - 1, params->y);
				continue;
			}
			
			if(x > params->x) 
			{
				change_position(params, params->x + 1, params->y);
				continue;
			}

			if(y > params->y) 
			{
				change_position(params, params->x, params->y + 1);
				continue;
			}

			if(y < params->y) {
				change_position(params, params->x, params->y - 1);
				continue;
			}
		}

		break;
	}
}

DWORD WINAPI ThreadVisiter(LPVOID Thread_id) 
{
	int * idptr = (int *) Thread_id; 
	int id = *idptr; 
	srand(Params[id]->time); 

	Sleep(Params[id]->time); 
	WaitForSingleObject(WriteMutex, INFINITE); 
	Params[id]->in_building = 1; 
	refresh_model(); 
	ReleaseMutex(WriteMutex); 

	get_to(Params[id], X_POS_FOR_TERMINAL + 10, Y_POS_FOR_TERMINAL + 6); 

	WaitForSingleObject(TerminalMutex, INFINITE); 
	int ticket_id = change_ticket_info(Params[id]->id); 
	
	WaitForSingleObject(WriteMutex, INFINITE); 
	refresh_ticket_window_info(); 
	ReleaseMutex(WriteMutex); 

	Sleep(TERMINAL_WORKING_TIME); 
	ReleaseMutex(TerminalMutex); 

	get_to(Params[id], MAIN_WINDOW_WIDTH/4 + id*2, MAIN_WINDOW_HEIGHT - 3); 

	WaitForSingleObject(WriteMutex, INFINITE); 
	Params[id]->in_building = 0; 
	ReleaseMutex(WriteMutex); 

	int index = WaitForMultipleObjects(WINDOWS_NUMBER, WindowMutexes, 0, INFINITE) - WAIT_OBJECT_0; 
	WaitForSingleObject(WriteMutex, INFINITE); 
	Params[id]->in_building = 1; 
	ReleaseMutex(WriteMutex); 
	int time = (rand() % 25 + 36) * 1000; 

	WaitForSingleObject(WriteMutex, INFINITE); 
	change_window_info(index, ticket_id, time / 1000, Params[id]->id); 
	write(" ", Params[id]->x, Params[id]->y); 
	Params[id]->x = WINDOW_WIDTH/2 + WINDOW_WIDTH*index;
	Params[id]->y = 4 + WINDOW_HEIGHT;
	refresh_model(); 
	ReleaseMutex(WriteMutex); 

	Sleep(time); 
	ReleaseMutex(WindowMutexes[index]); 

	WaitForSingleObject(WriteMutex, INFINITE); 
	write(" ", Params[id]->x, Params[id]->y); 
	Params[id]->x = MAIN_WINDOW_WIDTH/4 + id*2;
	Params[id]->y = MAIN_WINDOW_HEIGHT - 3;
	refresh_model(); 
	ReleaseMutex(WriteMutex); 
	Sleep(1000); 

	WaitForSingleObject(WriteMutex, INFINITE); 
	Params[id]->in_building = 0; 
	write(" ", Params[id]->x, Params[id]->y); 
	ReleaseMutex(WriteMutex); 

	ExitThread(0); 
}

void cls_scr()
{
	system("cls");
}
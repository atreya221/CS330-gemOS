#include "wc.h"

extern struct team teams[NUM_TEAMS];
extern int test;
extern int finalTeam1;
extern int finalTeam2;

int processType = HOST;

int grpApipe[2];
int grpBpipe[2];

const char *team_names[] = {
	"India", "Australia", "New Zealand", "Sri Lanka",   // Group A
	"Pakistan", "South Africa", "England", "Bangladesh" // Group B
};

void conductGroupMatches(void)
{
	int grpAprs, grpBprs;
	if(processType == HOST)
	{
		if(pipe(grpApipe) < 0)
		{
			perror("pipe");
			exit(-1);
		}
		grpAprs = fork();
		if(grpAprs != 0)
		{
			if(pipe(grpBpipe) < 0)
			{
				perror("pipe");
				exit(-1);
			}
			grpBprs = fork();
		}

		if(grpAprs!=0 && grpBprs!=0)
		{
			char buff1[2];
			char buff2[2];
			read(grpApipe[0], buff1, 1);
			read(grpBpipe[0], buff2, 1);
			buff1[1] = '\0';
			buff2[1] = '\0';
			finalTeam1 = atoi(buff1);
			finalTeam2 = atoi(buff2);
		}
		if(grpAprs == 0 || grpBprs == 0)
		{
			int grpAstats[4] = {0};
			int groupLeaderB=0;
			int groupLeaderA=0;
			int max1 = 0;
			int max2 = 0;
			if(grpAprs == 0 && grpBprs != 0 )
			{
				for(int i = 0; i < 3; i++)
				{
					for(int j = i+1; j < 4; j++)
					{
						int winner = match(i,j);
						if(winner==i)
						{
							grpAstats[i]++;
						}
						else if(winner==j)
						{
							grpAstats[j]++;
						}
					}
				}
				for(int i = 0; i < 4; i++)
				{
					if(max1 < grpAstats[i] )
					{
						max1 = grpAstats[i];
						groupLeaderA = i;
					}
				}

				for(int i = 0; i < 4; i++)
				{
					if(i != groupLeaderA)
					{
						endTeam(i);
					}
				}

				char buff[2];
				buff[0] = '0' + groupLeaderA;
				buff[1] = '\0';
				write(grpApipe[1], buff, 1);
				exit(0);
			}


			int grpBstats[4]={0};

			if(grpBprs == 0 && grpAprs != 0)
			{
				for(int i = 0; i < 3; i++)
				{
					for(int j = i+1; j < 4; j++)
					{
						int winner = match(i+4,j+4);
						if(winner == i+4)
						{
							grpBstats[i]++;
						}
						else if(winner == j+4)
						{
							grpBstats[j]++;
						}
					}
				}
				for(int i = 0; i < 4; i++)
				{
					if(max2 < grpBstats[i])
					{
						max2 = grpBstats[i];
						groupLeaderB = i+4;
					}
				}

				for(int i = 4; i < 8; i++)
				{
					if(i != groupLeaderB)
					{	
						endTeam(i);
					}
				}

				char buff2[2];
				buff2[0] = '0' + groupLeaderB;
				buff2[1] = '\0';

				write(grpBpipe[1], buff2, 1);
				exit(0);
			}
		}
	}
	return;
}


void teamPlay(void)
{
	char filepath[] = "test/";
	char res[20];

    sprintf(res, "%d", test);
	strcat(filepath, res);
	strcat(filepath, "/inp/");
	strcat(filepath, teams[processType].name);

	int fd = open(filepath, O_RDONLY);

	for(int i = 0; i < 1500; i++)
	{
		char matchbuff[2];
		char commbuff[2];
		read(fd,matchbuff,1);
		matchbuff[1]='\0';
		write(teams[processType].matchpipe[1],matchbuff,1);		
		write(teams[processType].commpipe[1],"0",1);	
		if(!strcmp(commbuff,"1"))
		{
			exit(0);
		}
	}
	if(processType != -1) 
	{
		exit(0);
	}
}

void endTeam(int teamID)
{
	char buff[2];
	buff[0] = '1';
	write(teams[teamID].commpipe[1], buff, 1);
	return;
}

int match(int team1, int team2)
{ 	
	char buff1[2];
	char buff2[2];

	read(teams[team1].matchpipe[0], buff1, 1);
	read(teams[team2].matchpipe[0], buff2, 1);

	int t1 = atoi(buff1), t2 = atoi(buff2);

	char *team1_name;
	char *team2_name;
	FILE* fd;

	char file_name[100] = {'\0'};
	if((t1 + t2) % 2 != 0)
	{
		team1_name = teams[team1].name;
		team2_name = teams[team2].name;
	}
	else
	{
		team2_name = teams[team1].name;
		team1_name = teams[team2].name;
		int temp = team1;
		team1 = team2;
		team2 = temp;
	}

	if((team1 < 4 && team2 >= 4) || (team1 >= 4 && team2 < 4 ))
	{
		strcat(file_name,team1_name);
		strcat(file_name,"v");
		strcat(file_name,team2_name);
		strcat(file_name,"-");
		strcat(file_name,"Final");
		strcat(file_name, "\0");
		fd = fopen(file_name, "w");
	}
	else
	{
		strcat(file_name,team1_name);
		strcat(file_name,"v");
		strcat(file_name,team2_name);
		strcat(file_name,"\0");
		fd = fopen(file_name, "w");
	}
	//dup2(fd,1);
	int num = 1;
	int sum = 0;
	int team1_sum=0;
	fprintf(fd, "Innings1: %s bats\n", team1_name);

	for(int i = 0; i < 120; i++)
	{
		char buff3[2];
		char buff4[2];

		read(teams[team1].matchpipe[0], buff3, 1);
		read(teams[team2].matchpipe[0], buff4, 1);
		buff3[1] = '\0';
		buff4[1] = '\0';
		int t3 = atoi(buff3), t4 = atoi(buff4);

		if(t3 != t4)
		{
			sum+=t3;
		}
		else
		{
			fprintf(fd, "%d:%d\n", num, sum);
			team1_sum += sum;
			sum = 0;
			num++;
		}
		if(num == 11) break;
	}
	if(num < 11)
	{
		fprintf(fd, "%d:%d*\n", num,sum);
	}
	fprintf(fd, "%s TOTAL: %d\n",team1_name, team1_sum);

	sum = 0;
	num = 1;
	int team2_sum = 0;
	int flag = 0;
	fprintf(fd, "Innings2: %s bats\n", team2_name);
	for(int i=0; i<120; i++)
	{
		char buff3[2];
		char buff4[2];
		read(teams[team1].matchpipe[0], buff3, 1);
		read(teams[team2].matchpipe[0], buff4, 1);

		buff3[1] = '\0';
		buff4[1] = '\0';
		int t3 = atoi(buff3), t4 = atoi(buff4);
		if(t3!=t4)
		{
			sum += t4;
			team2_sum += t4;
		}
		else
		{
			fprintf(fd, "%d:%d\n", num,sum);
			num++;
			sum = 0;
		}
		if(team1_sum < team2_sum)
		{
			fprintf(fd, "%d:%d*\n", num,sum);
			flag=1;
			break;
		}
		if(num == 11)
		{
			break;
		}
	}
	fprintf(fd, "%s TOTAL: %d\n",team2_name, team2_sum);
	if(team2_sum == team1_sum)
	{
		if(team1 < team2)
		{
			fprintf(fd, "%s beats %s\n", team1_name, team2_name);
			fclose(fd);
			return team1;
		}
		else
		{
			fprintf(fd, "%s beats %s\n", team2_name, team1_name);
			fclose(fd);
			return team2;
		}
	}
	else
	{
		if(flag == 0)
		{
			fprintf(fd, "%s beats %s by %d runs\n", team1_name, team2_name, team1_sum-team2_sum);
			fclose(fd);
			return team1;
		}
		else if(flag==1)
		{
			fprintf(fd, "%s beats %s by %d wickets\n", team2_name, team1_name, 11-num);
			fclose(fd);
			return team2;
		}
	}
	return 0;
}

void spawnTeams(void)
{
	for(int i = 0; i < 8; i++)
	{
		strcpy(teams[i].name, team_names[i]);
	}

	for(int i = 0; i < 8; i++)
	{
		if(pipe(teams[i].commpipe) < 0)
		{
			perror("pipe");
			exit(-1);
		}

		if(pipe(teams[i].matchpipe) < 0)
		{
			perror("pipe");
			exit(-1);
		}
		char buff[2] = {'0', '\0'};

		write(teams[i].commpipe[1], buff, 1);
		int team = -1;
		if(processType == HOST) team = fork();
		if(team == 0)
		{
			processType = i;
			teamPlay();
		}
	}
}
// --------------------------------------------------------------------------------
// Project Name		:	Platformer - Part 1
// File Name		:	BinaryMap.c
// Author			:	Sean Higgins
// Creation Date	:	Sept. 28 2016
// Purpose			:	Implementation of the binary map functions
// History			:
//	- 
//© Copyright 1996-2016, DigiPen Institute of Technology (USA). All rights reserved.
// ---------------------------------------------------------------------------------


#include "BinaryMap.h"



/*The number of horizontal elements*/
static int BINARY_MAP_WIDTH;

/*The number of vertical elements*/
static int BINARY_MAP_HEIGHT;

/*This will contain all the data of the map, which will be retireved from a file
when the "ImportMapDataFromFile" function is called*/
static int **MapData;

/*This will contain the collision dataof the binary map. It will be filled in the 
"ImportMapDataFromFile" after filling "MapData". Basically, if an array element 
in MapData is 1, it represents a collision cell, any other value is a non-collision
cell*/
static int **BinaryCollisionArray;



int GetCellValue(unsigned int X, unsigned int Y)
{
	//return 0;

	if (X < 0 || Y < 0 || X >= BINARY_MAP_HEIGHT || Y >= BINARY_MAP_WIDTH)
	{
		return 0;
	}

	else
	{
		return BinaryCollisionArray[X][Y];
	}
}

int CheckInstanceBinaryMapCollision(float PosX, float PosY, float scaleX, float scaleY)
{
	//return 0;
	float length2 = scaleY/ 2.f;
	float length4 = length2 / 2.f;
	float width2 = scaleX / 2.f;
	float width4 = width2 / 2.f;

	int topLeftX = (int)(PosX - width4);
	int topLeftY = (int)(PosY + length2);

	int topRightX = (int)(PosX + width4);
	int topRightY = topLeftY;

	int leftTopX = (int)(PosX - width2);
	int leftTopY = (int)(PosY + length4);

	int leftBotX = leftTopX;
	int leftBotY = (int)(PosY - length4);

	int botLeftX = topLeftX;
	int botLeftY = (int)(PosY - length2);

	int botRightX = topRightX;
	int botRightY = botLeftY;

	int rightTopX = (int)(PosX + width2);
	int rightTopY = leftTopY;

	int rightBotX = rightTopX;
	int rightBotY = leftBotY;

	int flag = 0;

	//Assuming that we're only checking the points on each side for its respective collision vs. dividing into quadrants
	if ((GetCellValue(topLeftX, topLeftY) | GetCellValue(topRightX, topRightY)) > 0)  //Might need to change these to explicitly check for 1 for both
	{
		flag |= COLLISION_TOP;
	}

	if ((GetCellValue(rightTopX, rightTopY) | GetCellValue(rightBotX, rightBotY)) > 0)
	{
		flag |= COLLISION_RIGHT;
	}

	if ((GetCellValue(leftTopX, leftTopY) | GetCellValue(leftBotX, leftBotY)) > 0)
	{
		flag |= COLLISION_LEFT;
	}

	if ((GetCellValue(botLeftX, botLeftY) | GetCellValue(botRightX, botRightY)) > 0)
	{
		flag |= COLLISION_BOTTOM;
	}

	return flag;

}

void SnapToCell(float *Coordinate)  //Would need to add a scale factor to this to make it work for non-unit-sized objects
{
	(*Coordinate) = ((int)(*Coordinate)) + 0.5f;

}

int ImportMapDataFromFile(char *FileName)
{
//	return 0;
	int w = -1;
	int l = -1;
	char trash[10];
	FILE* input;
	input = fopen(FileName, "r");
	if(input == NULL)
	{
		return 0;
	}

	else
	{
		fscanf(input,"%s %i", trash, &w);
		fscanf(input,"%s %i", trash, &l);



		if (w > 0 && l > 0)
		{
			BINARY_MAP_WIDTH = w;
			BINARY_MAP_HEIGHT = l;

			MapData = malloc(l*sizeof(int*));
			BinaryCollisionArray = malloc(l* sizeof(int*));
		


			for (int j = BINARY_MAP_HEIGHT - 1; j >= 0; --j)
			{


				for (int i = 0; i < BINARY_MAP_WIDTH; ++i)
				{
					if (j == BINARY_MAP_HEIGHT - 1)
					{
						BinaryCollisionArray[i] = malloc(w * sizeof(int));
						MapData[i] = malloc(w * sizeof(int));
					}

							fscanf(input,"%i", &MapData[i][j]);

							if (MapData[i][j] ==1)
							{
								BinaryCollisionArray[i][j] = 1;
							}

							else
							{
								BinaryCollisionArray[i][j] = 0;
							}
					
				}

				
			}



			//for(int i=BINARY_MAP_HEIGHT-1;i>=0;i--)//<BINARY_MAP_HEIGHT;i++)
			//{
			//	BinaryCollisionArray[i] = malloc(w * sizeof(int));
			//	MapData[i] = malloc(w * sizeof(int));

			//	for (int j =0;j<BINARY_MAP_WIDTH;j++)
			//	{
			//		
			//		fscanf(input,"%i", &MapData[j][i]);

			//		if (MapData[j][i] ==1)
			//		{
			//			BinaryCollisionArray[j][i] = 1;
			//		}

			//		else
			//		{
			//			BinaryCollisionArray[j][i] = 0;
			//		}
			//	}
			//}
			fclose(input);
			input = NULL;
			return 1;
		}

		else
		{
			return 0;
		}
	}


}

void FreeMapData(void)
{
	for(int i=0;i<BINARY_MAP_HEIGHT;i++)
	{
		free(BinaryCollisionArray[i]);
		free(MapData[i]);
	}

	free(BinaryCollisionArray);
	free(MapData);
}

void PrintRetrievedInformation(void)
{
	int i, j;

	printf("Width: %i\n", BINARY_MAP_WIDTH);
	printf("Height: %i\n", BINARY_MAP_HEIGHT);

	printf("Map Data:\n");
	for(j = BINARY_MAP_HEIGHT - 1; j >= 0; --j)
	{
		for(i = 0; i < BINARY_MAP_WIDTH; ++i)
		{
			printf("%i ", MapData[i][j]);
		}

		printf("\n");
	}

	printf("\n\nBinary Collision Data:\n");
	for(j = BINARY_MAP_HEIGHT - 1; j >= 0; --j)
	{
		for(i = 0; i < BINARY_MAP_WIDTH; ++i)
		{
			printf("%i ", BinaryCollisionArray[i][j]);
		}

		printf("\n");
	}
}
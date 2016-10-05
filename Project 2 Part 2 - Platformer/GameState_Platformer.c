// ---------------------------------------------------------------------------
// Project Name		:	Platformer
// File Name		:	GameState_Platformer.cpp
// Author			:	Antoine Abi Chakra
// Creation Date	:	2008/03/04
// Purpose			:	Implementation of the platform game state
// History			:
//	- 2015/09/28	:	Turned into a step by step project.
//  - 2015/12/10	:	Implemented C style component based architecture 
// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------


#include "AEEngine.h"
#include "GameStateMgr.h"

// ---------------------------------------------------------------------------

#define SHAPE_NUM_MAX				32					// The total number of different vertex buffer (Shape)
#define GAME_OBJ_INST_NUM_MAX		2048				// The total number of different game object instances


//Gameplay related variables and values
#define GRAVITY -20.0f
#define JUMP_VELOCITY 11.0f
#define MOVE_VELOCITY_HERO 4.0f
#define MOVE_VELOCITY_ENEMY 7.5f
#define ENEMY_IDLE_TIME 2.0
#define HERO_LIVES 3
static int HeroLives;
static int Hero_Initial_X;
static int Hero_Initial_Y;
static int TotalCoins;

//Flags
#define FLAG_ACTIVE			0x00000001


enum OBJECT_TYPE
{
	OBJECT_TYPE_MAP_CELL_EMPTY,			//0
	OBJECT_TYPE_MAP_CELL_COLLISION,		//1
	OBJECT_TYPE_HERO,					//2
	OBJECT_TYPE_ENEMY1,					//3
	OBJECT_TYPE_COIN					//4
};

//State machine states
enum STATE
{
	STATE_NONE,
	STATE_GOING_LEFT,
	STATE_GOING_RIGHT
};

//State machine inner states
enum INNER_STATE
{
	INNER_STATE_ON_ENTER,
	INNER_STATE_ON_UPDATE,
	INNER_STATE_ON_EXIT
};

// Struct/Class definitions

typedef struct GameObjectInstance GameObjectInstance;			// Forward declaration needed, since components need to point to their owner "GameObjectInstance"

// ---------------------------------------------------------------------------

typedef struct
{
	unsigned long			mType;				// Object type (Ship, bullet, etc..)
	AEGfxVertexList*		mpMesh;				// This will hold the triangles which will form the shape of the object

}Shape;

// ---------------------------------------------------------------------------

typedef struct
{
	Shape *mpShape;

	GameObjectInstance *	mpOwner;			// This component's owner
}Component_Sprite;

// ---------------------------------------------------------------------------

typedef struct
{
	AEVec2					mPosition;		// Current position
	float					mAngle;			// Current angle
	float					mScaleX;		// Current X scaling value
	float					mScaleY;		// Current Y scaling value

	AEMtx33					mTransform;		// Object transformation matrix: Each frame, calculate the object instance's transformation matrix and save it here

	GameObjectInstance *	mpOwner;		// This component's owner
}Component_Transform;

// ---------------------------------------------------------------------------

typedef struct
{
	AEVec2					mVelocity;		// Current velocity

	GameObjectInstance *	mpOwner;		// This component's owner
}Component_Physics;

// ---------------------------------------------------------------------------

typedef struct
{
	double					mCounter;		// Counter, used to wait before switching movement direction
	enum STATE				mState;			// Going left or right?
	enum INNER_STATE		mInnerState;	// On enter, On update or On exit?

	GameObjectInstance *	mpOwner;		// This component's owner
}Component_AI;

// ---------------------------------------------------------------------------

typedef struct
{
	unsigned int			mMapCollisionFlag;	// Bitfield, where each bit represents 1 side of the object

	GameObjectInstance *	mpOwner;			// This component's owner
}Component_CollisionWithMap;

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjectInstance
{
	unsigned long				mFlag;						// Bit mFlag, used to indicate if the object instance is active or not

	Component_Sprite			*mpComponent_Sprite;		// Sprite component
	Component_Transform			*mpComponent_Transform;		// Transform component
	Component_Physics			*mpComponent_Physics;		// Physics component
	Component_AI				*mpComponent_AI;			// AI, used by the enemy instances
	Component_CollisionWithMap	*mpComponent_MapCollision;	// Used by object instances that collides with the map
};

// ---------------------------------------------------------------------------

// List of original vertex buffers
static Shape				sgShapes[SHAPE_NUM_MAX];									// Each element in this array represents a unique shape 
static unsigned long		sgShapeNum;													// The number of defined shapes

// list of object instances
static GameObjectInstance		sgGameObjectInstanceList[GAME_OBJ_INST_NUM_MAX];		// Each element in this array represents a unique game object instance
static unsigned long			sgGameObjectInstanceNum;								// The number of active game object instances


// functions to create/destroy a game object instance
static GameObjectInstance*		GameObjectInstanceCreate(unsigned int ObjectType);		// From OBJECT_TYPE enum
static void						GameObjectInstanceDestroy(GameObjectInstance* pInst);

// ---------------------------------------------------------------------------




/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
// TO DO 1:
// -- Import these functions from part 1
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
//Binary map data
static int **MapData;
static int **BinaryCollisionArray;
static int BINARY_MAP_WIDTH;
static int BINARY_MAP_HEIGHT;
int GetCellValue(int X, int Y);
int CheckInstanceBinaryMapCollision(float PosX, float PosY, float scaleX, float scaleY);
void SnapToCell(float *Coordinate);
int ImportMapDataFromFile(char *FileName);
void FreeMapData(void);


static AEMtx33 sgMapTransform;


//Collision flags
#define	COLLISION_LEFT		0x00000001	//0001
#define	COLLISION_RIGHT		0x00000002	//0010
#define	COLLISION_TOP		0x00000004	//0100
#define	COLLISION_BOTTOM	0x00000008	//1000


// functions to create/destroy a game object instance
static GameObjectInstance*			GameObjectInstanceCreate(unsigned int ObjectType);			// From OBJECT_TYPE enum
static void							GameObjectInstanceDestroy(GameObjectInstance* pInst);

// ---------------------------------------------------------------------------

// Functions to add/remove components
static void AddComponent_Transform(GameObjectInstance *pInst, AEVec2 *pPosition, float Angle, float ScaleX, float ScaleY);
static void AddComponent_Sprite(GameObjectInstance *pInst, unsigned int ShapeType);
static void AddComponent_Physics(GameObjectInstance *pInst, AEVec2 *pVelocity);
static void AddComponent_AI(GameObjectInstance *pInst, double Counter, enum STATE State, enum INNER_STATE InnerState);
static void AddComponent_MapCollision(GameObjectInstance *pInst);

static void RemoveComponent_Transform(GameObjectInstance *pInst);
static void RemoveComponent_Sprite(GameObjectInstance *pInst);
static void RemoveComponent_Physics(GameObjectInstance *pInst);
static void RemoveComponent_AI(GameObjectInstance *pInst);
static void RemoveComponent_MapCollision(GameObjectInstance *pInst);

// ---------------------------------------------------------------------------

//We need a pointer to the hero's instance for input purposes
static GameObjectInstance *sgpHero;

//State machine functions
void EnemyStateMachine(GameObjectInstance *pInst);


void GameStatePlatformLoad(void)
{
	// Used for the cicle shape
	float circleAngleStep, i;
	int parts;
	Shape* pShape;

	circleAngleStep = 0.0f;
	i = 0.0f;
	parts = 0;
	pShape = NULL;

	// Zero the shapes array
	memset(sgShapes, 0, sizeof(Shape)* SHAPE_NUM_MAX);
	// No shapes at this point
	sgShapeNum = 0;

	//Creating the black object
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_MAP_CELL_EMPTY;

	AEGfxMeshStart();

	//1st argument: X
	//2nd argument: Y
	//3rd argument: ARGB
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF000000, 0.0f, 0.0f,
		 0.5f,  -0.5f, 0xFF000000, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFF000000, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFF000000, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFF000000, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFF000000, 0.0f, 0.0f);

	pShape->mpMesh = AEGfxMeshEnd();

	
	//Creating the white object
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_MAP_CELL_COLLISION;

	AEGfxMeshStart();

	//1st argument: X
	//2nd argument: Y
	//3rd argument: ARGB
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);

	pShape->mpMesh = AEGfxMeshEnd();

	
	//Creating the hero object
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_HERO;

	AEGfxMeshStart();

	//1st argument: X
	//2nd argument: Y
	//3rd argument: ARGB
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFF0000FF, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFF0000FF, 0.0f, 0.0f);

	pShape->mpMesh = AEGfxMeshEnd();


	//Creating the enemey1 object
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_ENEMY1;

	AEGfxMeshStart();

	//1st argument: X
	//2nd argument: Y
	//3rd argument: ARGB
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFFFF0000, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFFFF0000, 0.0f, 0.0f);

	pShape->mpMesh = AEGfxMeshEnd();


	//Creating the Coin object
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_COIN;

	AEGfxMeshStart();

	//1st argument: X
	//2nd argument: Y
	//3rd argument: ARGB

	//Creating the circle shape
	parts = 12;
	circleAngleStep = PI / parts; 
	for (i = 0; i < parts; ++i)
	{
		AEGfxTriAdd(
			0.0f, 0.0f, 0xFFFFFF00, 0.0f, 0.0f,
			cosf(i * 2 * circleAngleStep) *0.5f, sinf(i * 2 * circleAngleStep) *0.5f, 0xFFFFFF00, 0.0f, 0.0f,
			cosf((i + 1) * 2 * circleAngleStep) *0.5f, sinf((i + 1) * 2 * circleAngleStep) *0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	}

	pShape->mpMesh = AEGfxMeshEnd();

	//Setting intital binary map values
	MapData = 0;
	BinaryCollisionArray = 0;
	BINARY_MAP_WIDTH = 0;
	BINARY_MAP_HEIGHT = 0;

	//Importing Data
	if(!ImportMapDataFromFile("Exported.txt"))
		gGameStateNext = GS_QUIT;


	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 3:
	// -- Implement the map transform matrix
	//    A map translation and a scale are required. 
	// -- Store the just computed map transformation in "sgMapTransform"
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
}

void GameStatePlatformInit(void)
{
	int i, j;

	// zero the game object instance array
	memset(sgGameObjectInstanceList, 0, sizeof(GameObjectInstance)* GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sgGameObjectInstanceNum = 0;

	i = j = 0;
	sgpHero = 0;
	TotalCoins = 0;

	//Setting the inital number of hero lives
	HeroLives = HERO_LIVES;

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 2:
	// -- Create the map tiles.
	//    Loop through the loaded map array, and create a black instance (non collision) or white 
	//    instance (collision).
	// -- Create the objects instances (Hero, enemies & coins).
	// -- If implemented correctly, you should see the map and instances. (They'll be really small at
	//    this point
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// Creating the map, the main character, the enemies and the coins according to their initial positions in MapData

	/***********
	Loop through all the array elements of MapData (which was initialized in the "GameStatePlatformLoad" function
	from the .txt file
		
		 - Create a white or black cell

		 - if the element represents the hero
			Create a hero instance
			Set its position depending on its array indices in MapData
			Save its array indices in Hero_Initial_X and Hero_Initial_Y (Used when the hero dies and its position needs to be reset)

		 - if the element represents an enemy
			Create an enemy instance
			Set its position depending on its array indices in MapData
			
		 - if the element represents a coin
			Create a coin instance
			Set its position depending on its array indices in MapData



			
	***********/
	for(i = 0; i < BINARY_MAP_WIDTH; ++i)
		for(j = 0; j < BINARY_MAP_HEIGHT; ++j)
		{
		}
}

void GameStatePlatformUpdate(void)
{
	int i, j;
	float winMaxX, winMaxY, winMinX, winMinY;
	double frameTime;
	
	i = j = 0;
	winMaxX = winMaxY = winMinX = winMinY = 0.0f;
	// ==========================================================================================
	// Getting the window's world edges (These changes whenever the camera moves or zooms in/out)
	// ==========================================================================================
	winMaxX = AEGfxGetWinMaxX();
	winMaxY = AEGfxGetWinMaxY();
	winMinX = AEGfxGetWinMinX();
	winMinY = AEGfxGetWinMinY();


	// ======================
	// Getting the frame time
	// ======================

	frameTime = AEFrameRateControllerGetFrameTime();

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 8:
	// Control the hero's movement:
	// -- If Left/Right are pressed: Set the hero velocity's X coordinate to -/+ MOVE_VELOCITY_HERO.
	//    Set it to 0 when neither are pressed.
	// -- If SPACE is pressed AND the hero is on a platform: Jump. Use "JUMP_VELOCITY" as the upward
	//    jump velocity.
	// -- You should be able to move the main character and make it jump.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////

	if (AEInputCheckCurr(VK_LEFT))
	{
	}
	else
	if (AEInputCheckCurr(VK_RIGHT))
	{
	}
	else
	{
	}
	
	//if (AEInputCheckTriggered(VK_SPACE) && /*Hero is on a platform*/)
	//{
	//}



	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 6:
	// Update the velocity of each active game object instance that has a "Physics" component
	// -- velocities are updated here (V1.y = GRAVITY*t + V0.y)
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Update object instances physics and behavior
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// TO DO 10:
		// If the object instance is an enemy, update its state machine by calling the "EnemyStateMachine"
		// function.
		/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 5:
	// Update the positions of all active game object instances
	// -- Positions are updated here (P1 = V1*t + P0)
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 7:
	// -- Check for collision between active game object instances that have a "Component_CollisionWithMap"
	//	  component and the map, by calling the “CheckInstanceBinaryMapCollision”.
	//    Store the bit field in the instance's Component_CollisionWithMap.
	// -- In case of an intersection with the map, set the respective velocity coordinate to 0 and snap the 
	//    respective position coordinate.
	// -- If implemented correctly, object instances should stop falling down through the platforms.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Check for grid collision
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object instances
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;

	}


	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 11:
	// -- Check for collision among objects instances.
	//    Hero-Enemy intersection: Rectangle-Rectangle: The hero's position should be reset to its
	//		value
	//    Hero-Coin intersection: Rectangle-Circle: The coin should be deleted.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object instances
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;
	}

	
	//Computing the transformation matrices of the game object instances
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		AEMtx33 scale, rot, trans;
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;

		AEMtx33Scale(&scale, pInst->mpComponent_Transform->mScaleX, pInst->mpComponent_Transform->mScaleY);
		AEMtx33Rot(&rot, pInst->mpComponent_Transform->mAngle);
		AEMtx33Trans(&trans, pInst->mpComponent_Transform->mPosition.x, pInst->mpComponent_Transform->mPosition.y);

		AEMtx33Concat(&pInst->mpComponent_Transform->mTransform, &trans, &rot);
		AEMtx33Concat(&pInst->mpComponent_Transform->mTransform, &pInst->mpComponent_Transform->mTransform, &scale);
	}
}

void GameStatePlatformDraw(void)
{
	//Drawing the tile map (the grid)
	int i, j;
	AEMtx33 cellTranslation, cellFinalTransformation;
	double frameTime;

	i = j = 0;
	AEMtx33Identity(&cellTranslation);
	AEMtx33Identity(&cellFinalTransformation);

	// ======================
	// Getting the frame time
	// ======================

	frameTime = AEFrameRateControllerGetFrameTime();

	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxTextureSet(NULL, 0, 0);
	AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////B////////////////////////////////////////////////////////////////
	// TO DO 4:
	// -- Draw the map and all game object instances.
	// -- Concatenate "sgMapTransform" with the transformation matrix of each tile and game object
	//    instance.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////

	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object
		if (0 == (pInst->mFlag & FLAG_ACTIVE))
			continue;
		
		AEGfxSetTransform(pInst->mpComponent_Transform->mTransform.m);
		AEGfxMeshDraw(pInst->mpComponent_Sprite->mpShape->mpMesh, AE_GFX_MDM_TRIANGLES);
	}
}


void GameStatePlatformFree(void)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 12:
	//  -- Destroy all the active game object instances, using the “GameObjectInstanceDestroy” function.
	//  -- Reset the number of active game objects instances
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
}

void GameStatePlatformUnload(void)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 13:
	// -- Destroy all the shapes, using the “AEGfxMeshFree” function.
	// -- Free the map data
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
}


GameObjectInstance* GameObjectInstanceCreate(unsigned int ObjectType)			// From OBJECT_TYPE enum)
{
	unsigned long i;

	// loop through the object instance list to find a non-used object instance
	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// Check if current instance is not used
		if (pInst->mFlag == 0)
		{
			// It is not used => use it to create the new instance

			// Active the game object instance
			pInst->mFlag = FLAG_ACTIVE;

			pInst->mpComponent_Transform = 0;
			pInst->mpComponent_Sprite = 0;
			pInst->mpComponent_Physics = 0;
			pInst->mpComponent_AI = 0;

			// Add the components, based on the object type
			switch (ObjectType)
			{
			case OBJECT_TYPE_MAP_CELL_EMPTY:
				AddComponent_Sprite(pInst, OBJECT_TYPE_MAP_CELL_EMPTY);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				break;

			case OBJECT_TYPE_MAP_CELL_COLLISION:
				AddComponent_Sprite(pInst, OBJECT_TYPE_MAP_CELL_COLLISION);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				break;

			case OBJECT_TYPE_HERO:
				AddComponent_Sprite(pInst, OBJECT_TYPE_HERO);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				AddComponent_MapCollision(pInst);
				break;

			case OBJECT_TYPE_ENEMY1:
				AddComponent_Sprite(pInst, OBJECT_TYPE_ENEMY1);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				AddComponent_AI(pInst, 0, STATE_GOING_LEFT, INNER_STATE_ON_ENTER);
				AddComponent_MapCollision(pInst);
				break;

			case OBJECT_TYPE_COIN:
				AddComponent_Sprite(pInst, OBJECT_TYPE_COIN);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				AddComponent_MapCollision(pInst);
				break;
			}

			++sgGameObjectInstanceNum;

			// return the newly created instance
			return pInst;
		}
	}

	// Cannot find empty slot => return 0
	return 0;
}

// ---------------------------------------------------------------------------

void GameObjectInstanceDestroy(GameObjectInstance* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->mFlag == 0)
		return;

	// Zero out the mFlag
	pInst->mFlag = 0;

	RemoveComponent_Transform(pInst);
	RemoveComponent_Sprite(pInst);
	RemoveComponent_Physics(pInst);
	RemoveComponent_AI(pInst);
	RemoveComponent_MapCollision(pInst);

	--sgGameObjectInstanceNum;
}

// ---------------------------------------------------------------------------

void AddComponent_Transform(GameObjectInstance *pInst, AEVec2 *pPosition, float Angle, float ScaleX, float ScaleY)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Transform)
		{
			pInst->mpComponent_Transform = (Component_Transform *)calloc(1, sizeof(Component_Transform));
		}

		AEVec2 zeroVec2;
		AEVec2Zero(&zeroVec2);

		pInst->mpComponent_Transform->mScaleX = ScaleX;
		pInst->mpComponent_Transform->mScaleY = ScaleY;
		pInst->mpComponent_Transform->mPosition = pPosition ? *pPosition : zeroVec2;;
		pInst->mpComponent_Transform->mAngle = Angle;
		pInst->mpComponent_Transform->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_Sprite(GameObjectInstance *pInst, unsigned int ShapeType)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Sprite)
		{
			pInst->mpComponent_Sprite = (Component_Sprite *)calloc(1, sizeof(Component_Sprite));
		}

		pInst->mpComponent_Sprite->mpShape = sgShapes + ShapeType;
		pInst->mpComponent_Sprite->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_Physics(GameObjectInstance *pInst, AEVec2 *pVelocity)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Physics)
		{
			pInst->mpComponent_Physics = (Component_Physics *)calloc(1, sizeof(Component_Physics));
		}

		AEVec2 zeroVec2;
		AEVec2Zero(&zeroVec2);

		pInst->mpComponent_Physics->mVelocity = pVelocity ? *pVelocity : zeroVec2;
		pInst->mpComponent_Physics->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_AI(GameObjectInstance *pInst, double Counter, enum STATE State, enum INNER_STATE InnerState)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_AI)
		{
			pInst->mpComponent_AI = (Component_AI *)calloc(1, sizeof(Component_AI));
		}

		pInst->mpComponent_AI->mCounter = Counter;
		pInst->mpComponent_AI->mState = State;
		pInst->mpComponent_AI->mInnerState = InnerState;
		pInst->mpComponent_AI->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_MapCollision(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_MapCollision)
		{
			pInst->mpComponent_MapCollision = (Component_CollisionWithMap *)calloc(1, sizeof(Component_CollisionWithMap));
		}

		pInst->mpComponent_MapCollision->mMapCollisionFlag = 0;
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Transform(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Transform)
		{
			free(pInst->mpComponent_Transform);
			pInst->mpComponent_Transform = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Sprite(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Sprite)
		{
			free(pInst->mpComponent_Sprite);
			pInst->mpComponent_Sprite = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Physics(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Physics)
		{
			free(pInst->mpComponent_Physics);
			pInst->mpComponent_Physics = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_AI(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_AI)
		{
			free(pInst->mpComponent_AI);
			pInst->mpComponent_AI = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_MapCollision(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_MapCollision)
		{
			free(pInst->mpComponent_MapCollision);
			pInst->mpComponent_MapCollision = 0;
		}
	}
}

// ---------------------------------------------------------------------------

int GetCellValue(int X, int Y)
{
	return 0;
}

// ---------------------------------------------------------------------------

int CheckInstanceBinaryMapCollision(float PosX, float PosY, float scaleX, float scaleY)
{
	//At the end of this function, "Flag" will be used to determine which sides
	//of the object instance are colliding. 2 hot spots will be placed on each side.
	return 0;
}

// ---------------------------------------------------------------------------

void SnapToCell(float *Coordinate)
{

}

// ---------------------------------------------------------------------------

int ImportMapDataFromFile(char *FileName)
{
	return 0;
}

// ---------------------------------------------------------------------------

void FreeMapData(void)
{

}

// ---------------------------------------------------------------------------

void EnemyStateMachine(GameObjectInstance *pInst)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 9:
	// -- Implement the enemy’s state machine.
	// -- Each enemy’s current movement status is controlled by its “state”, “innerState” and 
	//    “counter” member variables.
	//    Refer to the provided enemy movement flowchart.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
}

// ---------------------------------------------------------------------------

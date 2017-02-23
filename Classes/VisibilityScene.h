#ifndef VISIBILITYSCENE_H
#define VISIBILITYSCENE_H

#include <cocos2d.h>
#include "CustomNode.h"
#include "ECS.h"
#include "Component.h"
#include "System.h"
#include "EarClippingScene.h"

struct UniquePoint
{
	int wallID;
	cocos2d::Vec2 point;
};

struct Segment
{
	cocos2d::Vec2 p1;
	cocos2d::Vec2 p2;
	int wallID;						// ID for wall. Uses ECS's entity id.
};

struct Vertex
{
	enum class TYPE
	{
		ON_BOUNDARY,
		ON_WALL,
		ON_UNIQUE_POINT
	};
	cocos2d::Vec2 point;
	cocos2d::Vec2 uniquePoint;
	TYPE type;
	float angle;
	int uniquePointWallID;
	int pointWallID;
};

struct VertexComparator
{
	bool operator()(const Vertex& a, const Vertex& b)
	{
		return a.angle < b.angle;
	}
};

struct Wall
{
	std::vector<ECS::Entity*> entities;
	std::vector<cocos2d::Vec2> points;
	cocos2d::Rect bb;
	cocos2d::Vec2 center;
	int wallID;
	float angle;
	bool rectangle;
};

struct Hit
{
	float t;
	float u;
	cocos2d::Vec2 hitPoint;
	bool parallel;
};

class VisibilityScene : public cocos2d::Scene
{
private:
	//default constructor
	VisibilityScene() {};

	//Input Listeners
	cocos2d::EventListenerMouse* mouseInputListener;
	cocos2d::EventListenerKeyboard* keyInputListener;

	//Mouse events
	void onMouseMove(cocos2d::Event* event);
	void onMouseDown(cocos2d::Event* event);
	void onMouseUp(cocos2d::Event* event);
	void onMouseScroll(cocos2d::Event* event);

	//keyboard events
	void onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event);
	void onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event);

	//cocos2d virtuals
	virtual bool init() override;
	virtual void onEnter() override;
	virtual void update(float delta) override;
	virtual void onExit() override;

	void initInputListeners();
	void releaseInputListeners();

	enum Z_ORDER
	{
		WALL,
		TRIANGLE,
		RAYCAST,
		BOX,
		DRAG,
		FREEFORM,
	};

	enum class CUSTOM_LABEL_INDEX
	{
		STATUS,
		MODE
	};

	enum class MODE
	{
		IDLE,
		DRAW_WALL_READY,
		DRAW_WALL_DRAG,
		DRAW_WALL_POINT,
	};
	
	MODE currentMode;

	bool draggingBox;
	const int maxWallPoints = 128;
	const int maxLightCount = 16;
	const float maxWallSegmentSize = 100.0f;
	const float minRectSize = 10.0f;
	const int BOUNDARY_WALL_ID = -1;
	static int wallIDCounter;

	EarClippingScene* earClipping;

	cocos2d::DrawNode* visiableAreaDrawNode;

	cocos2d::Vec2 newBoxOrigin;
	cocos2d::Vec2 newBoxDest;
	cocos2d::DrawNode* dragBoxDrawNode;
	cocos2d::DrawNode* wallDrawNode;

	cocos2d::DrawNode* freeformWallDrawNode;
	std::vector<cocos2d::Vec2> freeformWallPoints;

	LabelsNode* labelsNode;
	DisplayBoundaryBoxNode* displayBoundaryBoxNode;
	cocos2d::Rect displayBoundary;

	cocos2d::DrawNode* raycastDrawNode;
	cocos2d::DrawNode* triangleDrawNode;
	cocos2d::Vec2 mousePos;

	std::vector<Vertex> intersects;

	std::vector<cocos2d::Vec2> lightPositions;

	std::vector<UniquePoint> wallUniquePoints;
	std::vector<cocos2d::Vec2> boundaryUniquePoints;

	std::vector<Segment*> wallSegments;
	std::vector<Segment*> boundarySegments;
	
	std::vector<Wall> walls;

	cocos2d::RenderTexture* lightTexture;

	int hoveringWallIndex;

	bool viewRaycast;
	bool viewVisibleArea;
	bool cursorLight;

	// init ECS
	void initECS();
	// Create new box entity
	void createNewRectWall();
	// create new freeform wall
	void createNewFreeformWall();
	// create point
	ECS::Entity* createPoint(const cocos2d::Vec2& position);
	// Create new light entity
	void createNewLight(const cocos2d::Vec2& position);
	// load map
	void loadMap();
	// load rect
	void loadRect(const cocos2d::Rect& rect, std::vector<Segment*>& segments, const int wallID);
	// load freeform
	void loadFreeform(const std::vector<cocos2d::Vec2>& points, std::vector<Segment*>& segments, const int wallID);
	// load segments
	void addSegment(const cocos2d::Vec2& p1, const cocos2d::Vec2& p2, std::vector<Segment*>& segments, const int wallID);
	// Get intersecting point where raycast hits
	float getIntersectingPoint(const cocos2d::Vec2& rayStart, const cocos2d::Vec2& rayEnd, const Segment* segment, cocos2d::Vec2& intersection);
	cocos2d::Vec2 getIntersectingPoint(const cocos2d::Vec2& rayStart, const cocos2d::Vec2& rayEnd, const cocos2d::Vec2& segStart, const cocos2d::Vec2& segEnd);
	bool getIntersectingPoint(const cocos2d::Vec2& rayStart, const cocos2d::Vec2& rayEnd, const Segment* segment, Hit& hit);
	// cast rays
	void findIntersectsWithRaycasts();
	// draw triangles
	void drawTriangles();
	// Check if mouse point is inside of wall
	bool isPointInWall(const cocos2d::Vec2& point);
	// generateLightTexture
	void generateLightTexture();
	// Draw new wall preview
	void drawDragBox();
	// clear drag
	void clearDrag();
	// Draw Freeform wall
	void drawFreeformWall();
	// clear freeform
	void clearFreeform();
	// draw walls
	void drawWalls();
	// draw raycast
	void drawRaycast();
	// check if point is on left or right of segment
	bool isOnLeft(const cocos2d::Vec2& p1, const cocos2d::Vec2& p2, const cocos2d::Vec2& target);
	// Check if ray hit other wall
	bool didRayHitOtherWall(const std::unordered_set<int>& wallIDSet, const int uniquePointWallID);
public:
	//simple creator func
	static VisibilityScene* createScene();

	//default destructor
	~VisibilityScene() {};

	//Cocos2d Macro
	CREATE_FUNC(VisibilityScene);
};

#endif
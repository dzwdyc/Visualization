#include "VisibilityScene.h"
#include "MainScene.h"

USING_NS_CC;

int VisibilityScene::wallIDCounter = 0;

VisibilityScene* VisibilityScene::createScene()
{
	VisibilityScene* newVisibilityScene = VisibilityScene::create();
	return newVisibilityScene;
}

bool VisibilityScene::init()
{
	if (!cocos2d::Scene::init())
	{
		return false;
	}

	this->scheduleUpdate();

	// Use for freeform walls(polygon)
	earClipping = EarClippingScene::createScene();
	earClipping->retain();

	ECS::Manager::getInstance();

	// init display boundary box node which draws outer line of simulation display box
	this->displayBoundaryBoxNode = DisplayBoundaryBoxNode::createNode();
	this->displayBoundaryBoxNode->setPosition(cocos2d::Vec2::ZERO);
	this->displayBoundaryBoxNode->displayBoundary = cocos2d::Rect(0, 0, 650, 650);
	this->displayBoundaryBoxNode->drawDisplayBoundaryBox();
	this->displayBoundary = this->displayBoundaryBoxNode->displayBoundary;
	this->displayBoundaryBoxNode->retain();
	this->displayBoundaryBoxNode->drawNode->setLocalZOrder(static_cast<int>(Z_ORDER::BOX));
	this->addChild(this->displayBoundaryBoxNode);

	// Init labels node
	this->labelsNode = LabelsNode::createNode();
	this->labelsNode->setSharedLabelPosition(LabelsNode::SHARED_LABEL_POS_TYPE::QUADTREE_SCENE);
	this->addChild(this->labelsNode);

	auto winSize = cocos2d::Director::getInstance()->getVisibleSize();

	// Starting pos
	float labelX = winSize.height - 10.0f;
	float labelY = winSize.height - 45.0f;

	// Set title
	this->labelsNode->initTitleStr("Visibility", cocos2d::Vec2(labelX, labelY));

	labelY -= 50.0f;

	// Init custom labels
	this->labelsNode->customLabelStartPos = cocos2d::Vec2(labelX, labelY);

	// Set size
	const int customLabelSize = 25;

	this->labelsNode->addLabel(LabelsNode::TYPE::CUSTOM, "Status: Idle", customLabelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::CUSTOM, "Mode: Idle", customLabelSize);

	this->currentMode = MODE::IDLE;
	this->draggingBox = false;
	this->newBoxOrigin = cocos2d::Vec2::ZERO;
	this->newBoxDest = cocos2d::Vec2::ZERO;
	this->dragBoxDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->dragBoxDrawNode, Z_ORDER::DRAG);

	this->visiableAreaDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->visiableAreaDrawNode);

	this->raycastDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->raycastDrawNode, Z_ORDER::RAYCAST);
	this->triangleDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->triangleDrawNode, Z_ORDER::TRIANGLE);
	this->freeformWallDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->freeformWallDrawNode, Z_ORDER::FREEFORM);
	this->wallDrawNode = cocos2d::DrawNode::create();
	this->addChild(this->wallDrawNode, Z_ORDER::WALL);

	this->mousePos = cocos2d::Vec2::ZERO;

	this->hoveringWallIndex = -1;

	this->viewRaycast = true;
	this->viewVisibleArea = true;
	this->cursorLight = false;

	return true;
}

void VisibilityScene::onEnter()
{
	cocos2d::Scene::onEnter();

	initECS();

	initInputListeners();
}

void VisibilityScene::initECS()
{
	auto m = ECS::Manager::getInstance();
	m->createEntityPool("WALL_POINT", maxWallPoints);
	m->createEntityPool("LIGHT", maxLightCount);
}

void VisibilityScene::createNewRectWall()
{
	cocos2d::Vec2 newWallSize = this->newBoxDest - this->newBoxOrigin;

	auto maxX = this->newBoxOrigin.x > this->newBoxDest.x ? this->newBoxOrigin.x : this->newBoxDest.x;
	auto minX = this->newBoxOrigin.x < this->newBoxDest.x ? this->newBoxOrigin.x : this->newBoxDest.x;
	auto maxY = this->newBoxOrigin.y > this->newBoxDest.y ? this->newBoxOrigin.y : this->newBoxDest.y;
	auto minY = this->newBoxOrigin.y < this->newBoxDest.y ? this->newBoxOrigin.y : this->newBoxDest.y;

	cocos2d::Rect rect = cocos2d::Rect(cocos2d::Vec2(minX, minY), cocos2d::Size(maxX - minX, maxY - minY));

	auto topLeft = this->createPoint(cocos2d::Vec2(rect.getMinX(), rect.getMaxY()));  //top left
	auto topRight = this->createPoint(cocos2d::Vec2(rect.getMaxX(), rect.getMaxY()));  //top right
	auto bottomLeft = this->createPoint(cocos2d::Vec2(rect.getMinX(), rect.getMinY()));  //bottom left
	auto bottomRight = this->createPoint(cocos2d::Vec2(rect.getMaxX(), rect.getMinY()));  //bottom right

	Wall wall;
	wall.angle = 0;
	wall.center = cocos2d::Vec2(rect.getMidX(), rect.getMidY());
	wall.entities.push_back(topLeft);
	wall.entities.push_back(topRight);
	wall.entities.push_back(bottomRight);
	wall.entities.push_back(bottomLeft);
	wall.points.push_back(cocos2d::Vec2(rect.getMinX(), rect.getMaxY()));  //top left
	wall.points.push_back(cocos2d::Vec2(rect.getMaxX(), rect.getMaxY()));  //top right
	wall.points.push_back(cocos2d::Vec2(rect.getMaxX(), rect.getMinY()));  //bottom right
	wall.points.push_back(cocos2d::Vec2(rect.getMinX(), rect.getMinY()));  //bottom left
	wall.wallID = VisibilityScene::wallIDCounter++;
	wall.bb = rect;
	wall.rectangle = true;

	this->walls.push_back(wall);

	this->drawWalls();
}

void VisibilityScene::createNewFreeformWall()
{
	Wall wall;
	wall.angle = 0;

	float maxX = 0;
	float minX = 0;
	float maxY = 0;
	float minY = 0;

	for (auto point : this->freeformWallPoints)
	{
		auto newPointEntity = this->createPoint(point);
		wall.entities.push_back(newPointEntity);
		wall.points.push_back(point);

		if (point.x > maxX)
		{
			maxX = point.x;
		}
		else if (point.x < minX)
		{
			minX = point.x;
		}

		if (point.y > maxY)
		{
			maxY = point.y;
		}
		else if (point.y < minY)
		{
			minY = point.y;
		}
	}

	cocos2d::Rect rect = cocos2d::Rect(cocos2d::Vec2(minX, minY), cocos2d::Size(maxX - minX, maxY - minY));

	wall.bb = rect;
	wall.center = cocos2d::Vec2(rect.getMidX(), rect.getMidY());
	wall.rectangle = false;

	wall.wallID = VisibilityScene::wallIDCounter++;

	this->walls.push_back(wall);

	this->drawWalls();
}

ECS::Entity* VisibilityScene::createPoint(const cocos2d::Vec2& position)
{
	auto m = ECS::Manager::getInstance();
	auto newWallPoint = m->createEntity("WALL_POINT");

	auto spriteComp = m->createComponent<ECS::Sprite>();
	spriteComp->sprite = cocos2d::Sprite::createWithSpriteFrameName("circle.png");
	spriteComp->sprite->setColor(cocos2d::Color3B::BLUE);
	spriteComp->sprite->setPosition(position);
	spriteComp->sprite->runAction(cocos2d::ScaleTo::create(0.25f, 0));
	this->addChild(spriteComp->sprite, Z_ORDER::WALL);

	newWallPoint->addComponent<ECS::Sprite>(spriteComp);

	return newWallPoint;
}

void VisibilityScene::createNewLight(const cocos2d::Vec2& position)
{
	auto m = ECS::Manager::getInstance();
	auto newLight = m->createEntity("LIGHT");

	auto spriteComp = m->createComponent<ECS::Sprite>();
	spriteComp->sprite = cocos2d::Sprite::createWithSpriteFrameName("circle.png");
	spriteComp->sprite->setPosition(position);
	this->addChild(spriteComp->sprite);

	newLight->addComponent<ECS::Sprite>(spriteComp);

	this->lightPositions.push_back(position);
}

void VisibilityScene::loadMap()
{
	// clear all
	this->wallUniquePoints.clear();
	this->boundaryUniquePoints.clear();

	for (auto segment : this->wallSegments)
	{
		if (segment) delete segment;
	}

	this->wallSegments.clear();

	for (auto segment : this->boundarySegments)
	{
		if (segment) delete segment;
	}

	this->boundarySegments.clear();

	// Add boundary
	this->boundaryUniquePoints.push_back(cocos2d::Vec2(this->displayBoundary.getMinX(), this->displayBoundary.getMaxY()));  //top left
	this->boundaryUniquePoints.push_back(cocos2d::Vec2(this->displayBoundary.getMaxX(), this->displayBoundary.getMaxY()));  //top right
	this->boundaryUniquePoints.push_back(cocos2d::Vec2(this->displayBoundary.getMinX(), this->displayBoundary.getMinY()));  //bottom left
	this->boundaryUniquePoints.push_back(cocos2d::Vec2(this->displayBoundary.getMaxX(), this->displayBoundary.getMinY()));  //bottom right

	this->loadRect(this->displayBoundary, this->boundarySegments, -1/*boundary isn't wall. so use -1*/);

	// Add walls
	for (auto wall : walls)
	{
		if (wall.rectangle)
		{
			auto& bb = wall.bb;

			this->wallUniquePoints.push_back(cocos2d::Vec2(bb.getMinX(), bb.getMaxY()));  //top left
			this->wallUniquePoints.push_back(cocos2d::Vec2(bb.getMaxX(), bb.getMaxY()));  //top right
			this->wallUniquePoints.push_back(cocos2d::Vec2(bb.getMinX(), bb.getMinY()));  //bottom left
			this->wallUniquePoints.push_back(cocos2d::Vec2(bb.getMaxX(), bb.getMinY()));  //bottom right

			this->loadRect(bb, this->wallSegments, wall.wallID);
		}
		else
		{
			auto size = wall.points.size();
			for (unsigned int i = 0; i < size; i++)
			{
				this->wallUniquePoints.push_back(wall.points.at(i));
			}

			this->loadFreeform(wall.points, this->wallSegments, wall.wallID);
		}
	}
}

void VisibilityScene::loadRect(const cocos2d::Rect& rect, std::vector<Segment*>& segments, const int wallID)
{
	// Going clock wise from top left point, first point is begin and second point is not

	cocos2d::Vec2 topLeft = cocos2d::Vec2(rect.getMinX(), rect.getMaxY());
	cocos2d::Vec2 topRight = cocos2d::Vec2(rect.getMaxX(), rect.getMaxY());
	cocos2d::Vec2 bottomLeft = cocos2d::Vec2(rect.getMinX(), rect.getMinY());
	cocos2d::Vec2 bottomRight = cocos2d::Vec2(rect.getMaxX(), rect.getMinY());

	// Top segment
	this->addSegment(topLeft, topRight, segments, wallID);

	// right segment
	this->addSegment(topRight, bottomRight, segments, wallID);

	// bottom segment
	this->addSegment(bottomRight, bottomLeft, segments, wallID);

	// Left segment
	this->addSegment(bottomLeft, topLeft, segments, wallID);
}

void VisibilityScene::loadFreeform(const std::vector<cocos2d::Vec2>& points, std::vector<Segment*>& segments, const int wallID)
{
	auto size = points.size();
	for (unsigned int i = 0; i < size; i++)
	{
		if (i < size - 1)
		{
			this->addSegment(points.at(i), points.at(i + 1), segments, wallID);
		}
	}

	if (size > 1)
	{
		this->addSegment(points.at(0), points.at(size - 1), segments, wallID);
	}
}

void VisibilityScene::addSegment(const cocos2d::Vec2& p1, const cocos2d::Vec2& p2, std::vector<Segment*>& segments, const int wallID)
{
	Segment* segment = new Segment();

	segment->p1 = p1;
	segment->p2 = p2;

	segment->wallID = wallID;

	segments.push_back(segment);

	//cocos2d::log("Added segment p1: (%f, %f), p2: (%f, %f)", p1.x, p1.y, p2.x, p2.y);
}

float VisibilityScene::getIntersectingPoint(const cocos2d::Vec2 & rayStart, const cocos2d::Vec2 & rayEnd, const Segment * segment, cocos2d::Vec2 & intersection)
{
	auto& rs = rayStart;
	auto& re = rayEnd;

	auto& sp1 = segment->p1;
	auto& sp2 = segment->p2;

	auto rd = re - rs;
	auto sd = sp2 - sp1;

	auto rdDotsd = rd.dot(sd);

	auto rsd = sp1 - rs;

	auto rdxsd = rd.cross(sd);		//denom
	auto rsdxrd = rsd.cross(rd);

	float t = rsd.cross(sd) / rdxsd;
	float u = rsd.cross(rd) / rdxsd;

	cocos2d::log("u = %f", u);
	cocos2d::log("t = %f", t);

	if (rdxsd == 0)
	{
		/*
		if (rsdxrd == 0)
		{
			cocos2d::log("colinear");
		}
		else
		{
			cocos2d::log("Parallel and not intersecting");
		}
		*/

		intersection = cocos2d::Vec2::ZERO;
		return -1;
	}
	else
	{
		if (u > 1.0f && u - 1.0f < 0.00001f)
		{
			u = 1.0f;
		}
		if (0 <= t &&  0 <= u && u <= 1.0f)
		{
			//cocos2d::log("Intersecting");
			intersection.x = rayStart.x + (t * rd.x);
			intersection.y = rayStart.y + (t * rd.y);
			intersection.x = sp1.x + (u * sd.x);
			intersection.y = sp1.y + (u * sd.y);
			return t;
		}
	}

	//cocos2d::log("Not colinear nor parallel, but doesn't intersect");
	return 0;
}

cocos2d::Vec2 VisibilityScene::getIntersectingPoint(const cocos2d::Vec2 & p1, const cocos2d::Vec2 & p2, const cocos2d::Vec2 & p3, const cocos2d::Vec2 & p4)
{
	float s = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x))
		/ ((p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y));
	return cocos2d::Vec2(p1.x + s * (p2.x - p1.x), p1.y + s * (p2.y - p1.y));
}

void VisibilityScene::findIntersectsWithRaycasts()
{
	// Don't need to find if there is no light
	if (this->lightPositions.empty()) return;

	//auto lightPos = this->lightPositions.at(0);
	auto lightPos = mousePos;	//debug

	if (this->displayBoundary.containsPoint(lightPos))
	{
		// load the map and generate segments.
		this->loadMap();

		// clear intersecting points.
		intersects.clear();

		// Iterate through wall unique point and calculate angles
		std::vector<float> wallUniqueAngles;
		for (auto uniquePoint : this->wallUniquePoints)
		{
			float angle = atan2(uniquePoint.y - lightPos.y, uniquePoint.x - lightPos.x);
			wallUniqueAngles.push_back(angle);
		}

		// Iterate through boundary unique point and calculate angles
		std::vector<float> boundaryUniqueAngles;
		for (auto uniquePoint : this->boundaryUniquePoints)
		{
			float angle = atan2(uniquePoint.y - lightPos.y, uniquePoint.x - lightPos.x);
			boundaryUniqueAngles.push_back(angle);
		}

		// So at this point, we have all unique point and angle calculated.

		// Now iterate the angle we calculated above, create ray and see if hits any segments
		std::vector<Segment*> allSegments;
		for (auto s : this->boundarySegments)
		{
			allSegments.push_back(s);
		}

		for (auto s : this->wallSegments)
		{
			allSegments.push_back(s);
		}

		// Eventhough we use both boudary and wall segment for raycasting, we need separate 
		// list of unique points to determine if we really need the intersecting point.
		// More details on below comments

		int wallIndex = 0;	// for unique point

		// Iterate through walls first
		for (auto angle : wallUniqueAngles)
		{
			int hitCount = 0;	// increment everytime ray hits segment

			// Get direction of ray
			float dx = cosf(angle);
			float dy = sinf(angle);

			// Generate raycast vec2 points
			auto rayStart = lightPos;
			auto rayEnd = cocos2d::Vec2(rayStart.x + dx, rayStart.y + dy);

			// Zero will mean no hit
			cocos2d::Vec2 closestIntersection = cocos2d::Vec2::ZERO;
			cocos2d::Vec2 secondClosestIntersection = cocos2d::Vec2::ZERO;

			// This flag is for corner case where raycast is parallel to segment
			bool parallel = false;
			// Keep tracks the wallID of segment that raycast intersected.
			int wallID = -1;
			int uniquePointWallID = -1;

			// Iterate through all segments
			for (auto segment : allSegments)
			{
				// Convert to cocos2d::Vec2
				auto p1 = cocos2d::Vec2(segment->p1.x, segment->p1.y);
				auto p2 = cocos2d::Vec2(segment->p2.x, segment->p2.y);

				if (p1 == this->wallUniquePoints.at(wallIndex) || p2 == this->wallUniquePoints.at(wallIndex))
				{
					// Don't check segment that includes the unique point we are raycasting.
					uniquePointWallID = segment->wallID;
					continue;
				}

				// Get intersecting point
				cocos2d::Vec2 intersectingPoint;
				auto dist = this->getIntersectingPoint(rayStart, rayEnd, segment, intersectingPoint);

				// check the distance. if dist is 0, ray didn't hit any segments. If dist is -1, it's parallel to segment.
				if (dist > 0)
				{
					hitCount++;		// Increment counter

					wallID = segment->wallID;	// Keep track wallID

					if (closestIntersection == cocos2d::Vec2::ZERO)
					{
						// Haven't find any intersection yet. Set as closest
						closestIntersection = intersectingPoint;
					}
					else
					{
						// Check if new intersecting point we found is closer to light position(where ray starts)
						auto intersectDist = intersectingPoint.distance(rayStart);
						if (intersectDist < closestIntersection.distance(rayStart))
						{
							// If so, set as closest
							secondClosestIntersection = closestIntersection;
							closestIntersection = intersectingPoint;
						}
						else
						{
							if (uniquePointWallID != wallID)
							{
								if (secondClosestIntersection == cocos2d::Vec2::ZERO)
								{
									secondClosestIntersection = intersectingPoint;
								}
								else
								{
									if (intersectDist < secondClosestIntersection.distance(rayStart))
									{
										secondClosestIntersection = intersectingPoint;
									}
								}
							}
						}
					}
				}
				// else if dist == 0, didn't intersect. Do nothing.
				else if (dist < 0)
				{
					// it's parallel
					parallel = true;
				}
			}
			// end of segment interation

			if (hitCount == 1)
			{
				// Only hit once, which must be boundary segment.
				// In this case, it means that this intersecting point is visible from light and can be
				// extended to the boundary. 
				Vertex v;
				v.boundaryVisible = true;								// light can see boundary
				v.otherWallVisible = false;
				v.isBounday = false;									// It's wall not boundary
				v.vertex = this->wallUniquePoints.at(wallIndex);		// Ray hit the unique point. 
				v.extendedVertex = closestIntersection;					// But can be extended to boundary, which is closests point
				v.angle = angle;										// Store angle for sorting
				v.wallID = uniquePointWallID;							// Keep track which wall are we dealing with
				v.extendedWallID = -1;
				intersects.push_back(v);
			}
			else
			{
				// Ray hit more than 1 segment. This means ray hit boundary segment and additional wall segment(s)
				if (hitCount > 1)
				{
					// Check if closest intersecting point is closer than unique point
					auto dist = closestIntersection.distance(rayStart);
					auto maxDist = wallUniquePoints.at(wallIndex).distance(rayStart);
					if (dist > maxDist)
					{
						// Closest intersecting point is further than unique point from light position.
						// This means that there weren't any closests intersecting point than unique point.
						// Therefore, unique point will be the closest intersecting point.
						Vertex v;
						v.boundaryVisible = false;							// Light can't see the boundary
						v.isBounday = false;								// Therefore, it's not boundary
						v.vertex = wallUniquePoints.at(wallIndex);			// As said above, unique point is closest.

						if (parallel)
						{
							// The ray and segment was parallel.
							// hit count is always even number for parallel
							if (hitCount == 2)
							{
								// Didn't hit any other walls, just itself and boundary
								v.boundaryVisible = true;
								v.otherWallVisible = false;
								v.wallID = uniquePointWallID;
								v.extendedWallID = -1;
								v.extendedVertex = secondClosestIntersection;
							}
							else
							{
								// Hit some other walls before it reach the boundary.
								v.boundaryVisible = false;
								v.otherWallVisible = true;
								v.wallID = uniquePointWallID;
								v.extendedWallID = wallID;
								v.extendedVertex = secondClosestIntersection;
							}
						}
						else
						{
							// The ray wasn't parallel
							if (hitCount % 2 == 0)
							{
								// hit count is even. 
								// First of all, because we don't check hit with segments that contains unique point,
								// maximum number of hit that can be happen on the segments on same wll is 1. 
								// Also this means that maximum number of hit for single wall is 2.
								// So if the hit count is even number, which means
								// hitCount = boundary hit(1) + segment from same wall(1) + (number of walls that ray hit * 2)
								// So if hit count is even, vertex will be the same (unique point. see above comments)
								// and light can't see the boundary.
								v.boundaryVisible = false;
								v.otherWallVisible = false;
								v.extendedVertex = cocos2d::Vec2::ZERO;
								v.wallID = uniquePointWallID;
								v.extendedWallID = uniquePointWallID;
							}
							else
							{
								// hit count is odd.
								// As commented above, if hit count is odd, then hit count can be defined as
								// hitCount = boundary hit(1) + (number of walls that ray hit * 2)
								// Unlikely, when hit count is even, ray didn't hit any segments that are on same wall that unique point is.
								// So basically ray passed the unique point and some other walls and reached the boundary.
								// Important point is that, it surely hit the other wall(s), because hit count is odd but not 1.
								// Therefore, vertex remains the same (unique point. see above comments),
								// and light can see the other wall not boundary
								float degree = angle * 180.0f / M_PI;
								if (degree == 45.0f || degree == 135.0f || degree == -45.0f || degree == -135.0f)
								{
									// However, there are some corner cases.
									// If angle between segment and ray is 45, 135, -45, -135, there are two cases.
									// First, closest intersecting point is in same wall.
									// Second, closest intersecting point is boundary or segment from other wall.
									if (wallID == uniquePointWallID)
									{
										v.boundaryVisible = false;
										v.otherWallVisible = false;
										v.extendedVertex = cocos2d::Vec2::ZERO;
										v.wallID = uniquePointWallID;
										v.extendedWallID = uniquePointWallID;
									}
									else
									{
										v.boundaryVisible = false;
										v.otherWallVisible = true;
										v.extendedVertex = closestIntersection;
										v.wallID = uniquePointWallID;
										v.extendedWallID = wallID;
									}
									//cocos2d::log("corner case, hit count = %d", hitCount);
								}
								else
								{
									// If it's not a corner case,
									// As it said, boundary is not visible but wall
									v.boundaryVisible = false;
									v.otherWallVisible = true;
									v.extendedVertex = closestIntersection;
									v.wallID = uniquePointWallID;
									v.extendedWallID = wallID;
								}
							}
						}
						
						v.angle = angle;			// Angle for sorting
						intersects.push_back(v);
					}
					// Else, closest intersecting point is closer to light position that unique point.
					// This means that ray hit some segment before it reached the the unique point.
					// Then, it also means unique point is not visible from light. 
					// Therefore, we can ignore this interseting point because it's just extra.
				}
				// If it's 0, it's bug.
			}

			wallIndex++;
		}

		int boundaryIndex = 0;
		for (auto angle : boundaryUniqueAngles)
		{
			float dx = cosf(angle);
			float dy = sinf(angle);

			auto rayStart = lightPos;
			auto rayEnd = cocos2d::Vec2(rayStart.x + dx, rayStart.y + dy);

			// Zero will mean no hit
			cocos2d::Vec2 closestIntersection = cocos2d::Vec2::ZERO;

			for (auto segment : allSegments)
			{
				auto p1 = cocos2d::Vec2(segment->p1.x, segment->p1.y);
				auto p2 = cocos2d::Vec2(segment->p2.x, segment->p2.y);

				if (p1 == this->boundaryUniquePoints.at(boundaryIndex) || p2 == this->boundaryUniquePoints.at(boundaryIndex))
				{
					// Don't check segment that includes the unique point we are raycasting.
					continue;
				}

				cocos2d::Vec2 intersectingPoint;
				auto dist = this->getIntersectingPoint(rayStart, rayEnd, segment, intersectingPoint);

				if (dist != 0)
				{
					if (closestIntersection == cocos2d::Vec2::ZERO)
					{
						// Haven't find any intersection yet. Set as closest
						closestIntersection = intersectingPoint;
					}
					else
					{
						// Check if new intersecting point we found is closer to light position(where ray starts)
						if (intersectingPoint.distance(rayStart) < closestIntersection.distance(rayStart))
						{
							// If so, set as closest
							closestIntersection = intersectingPoint;
						}
					}
				}
			}

			if (closestIntersection == cocos2d::Vec2::ZERO)
			{
				// Ray didn't hit any segment. Closest intersecting point will be the unique point
				Vertex v;
				v.boundaryVisible = false;
				v.otherWallVisible = false;
				v.isBounday = true;
				v.vertex = boundaryUniquePoints.at(boundaryIndex);
				v.extendedVertex = cocos2d::Vec2::ZERO;
				v.angle = angle;
				v.wallID = -1;
				intersects.push_back(v);
			}
			// Else, ray hit segment. We ignore

			boundaryIndex++;
		}

		this->drawRaycast();
	}
}

void VisibilityScene::drawTriangles()
{
	this->triangleDrawNode->clear();

	if (this->viewVisibleArea == false) return;

	std::vector<cocos2d::Vec2> verticies;

	auto centerPos = this->mousePos;

	// Sort intersecting points by angle.
	std::sort(this->intersects.begin(), this->intersects.end(), VertexComparator());

	// Generate vertex for triangle. 
	auto size = this->intersects.size();
	bool prevEndedWithBoundayVisible = false;
	for (unsigned int i = 0; i < size; i++)
	{
		// First, add center position, which is position of light
		verticies.push_back(centerPos);		
		
		int index = i + 1;
		if (index == size)
		{
			index = 0;
		}

		auto& v2 = intersects.at(i);
		auto& v3 = intersects.at(index);

		// Second and thrid vertex have several cases.
		// The triangle we want to draw depends on each unique intersecting points.
		// Because light travels until hits the boundary, some unique points must be extended to the boundary.
		// Also, there are few corner cases 

		// Case #1
		if (v2.isBounday && v3.isBounday)
		{
			// v2 is boundary, v3 is boundary.
			// In this case, triangle is formed only with boundary unique points.
			verticies.push_back(v2.vertex);
			verticies.push_back(v3.vertex);
		}
		// Case #2
		else if (v2.isBounday && !v3.isBounday)
		{
			// v2 is boundary but v3 isn't.
			// In this case, v2 is boundary unique point and v3 is wall unique point.
			// Because light travel till boundary, use extendedBertex for v3.
			verticies.push_back(v2.vertex);
			verticies.push_back(v3.extendedVertex);
		}
		// Case #4
		else if (!v2.isBounday && v3.isBounday)
		{
			// v2 is not boundary but v3 is.
			// This is the opposite case of case #2. 
			verticies.push_back(v2.extendedVertex);
			verticies.push_back(v3.vertex);
		}
		// Case #3 (Corner cases)
		else
		{
			// v2 and v3 both are not boundary.
			// This means both v2 and v3 is wall unique point.
			if (v2.boundaryVisible && !v3.boundaryVisible)
			{
				// v2 can be extended to boundary but v3 isn't
				// Check if v3 can be extended to other wall
				verticies.push_back(v2.vertex);

				if (v3.otherWallVisible)
				{
					if (v2.wallID == v3.wallID)
					{
						verticies.push_back(v3.vertex);
					}
					else
					{
						verticies.push_back(v3.extendedVertex);
					}
				}
				else
				{
					verticies.push_back(v3.vertex);
				}

				continue;
			}
			
			if (!v2.boundaryVisible && v3.boundaryVisible)
			{
				// v2 cannot be extended to boundary but v3 can
				// check if v2 can extended to other wall
				if (v2.otherWallVisible)
				{
					if (v2.wallID == v3.wallID)
					{
						verticies.push_back(v2.vertex);
					}
					else
					{
						verticies.push_back(v2.extendedVertex);
					}
				}
				else
				{
					verticies.push_back(v2.vertex);
				}

				verticies.push_back(v3.vertex);

				continue;
			}

			if (v2.boundaryVisible && v3.boundaryVisible)
			{
				// both v2 and v3 can see boundary.
				// if two point is in same wall, use vertex. Else, extended vertex
				if (v2.wallID == v3.wallID)
				{
					verticies.push_back(v2.vertex);
					verticies.push_back(v3.vertex);
				}
				else
				{
					verticies.push_back(v2.extendedVertex);
					verticies.push_back(v3.extendedVertex);
				}

				continue;
			}
			
			if ((v2.otherWallVisible && !v3.otherWallVisible) || (!v2.otherWallVisible && v3.otherWallVisible))
			{
				// Case 3-4
				// If only either v2 or v3 can be extended to other wall, but not both, use extendedVertex if so.
				if (v2.otherWallVisible)
				{
					if (v2.wallID == v3.wallID)
					{
						verticies.push_back(v2.vertex);
					}
					else
					{
						verticies.push_back(v2.extendedVertex);
					}
				}
				else
				{
					verticies.push_back(v2.vertex);
				}

				if (v3.otherWallVisible)
				{
					if (v2.wallID == v3.wallID)
					{
						verticies.push_back(v3.vertex);
					}
					else
					{
						verticies.push_back(v3.extendedVertex);
					}
				}
				else
				{
					verticies.push_back(v3.vertex);
				}
			}
			else
			{
				// v2 and v3 is not boundary unique point, but either both can be extended or not.
				if (v2.wallID == v3.wallID)
				{
					// If both v2 and v3 is on same segemnt (same wall), use vertex
					verticies.push_back(v2.vertex);
					verticies.push_back(v3.vertex);
				}
				else
				{
					// If both v2 and v3 is not on same segment, check distance, 
					// First check if they ended up on same wall
					if (v2.extendedWallID == v3.extendedWallID)
					{
						verticies.push_back(v2.extendedVertex);
						verticies.push_back(v3.extendedVertex);
					}
					else
					{
						// Farther intersecting point uses vertex
						auto v2Dist = v2.extendedVertex.distance(centerPos);
						auto v3Dist = v3.extendedVertex.distance(centerPos);

						if (v2Dist < v3Dist)
						{
							// v2 is closer.
							verticies.push_back(v2.extendedVertex);
							verticies.push_back(v3.vertex);
						}
						else
						{
							verticies.push_back(v2.vertex);
							verticies.push_back(v3.extendedVertex);
						}
					}
				}
			}
		}
	}


	size = verticies.size();
	auto color = cocos2d::Color4F::WHITE;
	color.a = 0.8f;
	for (unsigned int i = 0; i < size; i+=3)
	{
		this->triangleDrawNode->drawTriangle(verticies.at(i), verticies.at(i + 1), verticies.at(i + 2), color);
	}
}

bool VisibilityScene::isPointInWall(const cocos2d::Vec2 & point)
{
	for (auto wall : this->walls)
	{
		if (wall.rectangle)
		{
			if (wall.bb.containsPoint(point))
			{
				return true;
			}
		}
		else
		{
			auto size = wall.points.size();
			if (size >= 3)
			{
				std::list<cocos2d::Vec2> pointList(wall.points.begin(), wall.points.end());

				bool inPolygon = earClipping->isPointInPolygon(pointList, point);

				if (inPolygon)
				{
					return true;
				}
			}
			else
			{
				// it's a line
				continue;
			}
		}
	}

	return false;
}

void VisibilityScene::drawDragBox()
{
	this->dragBoxDrawNode->clear();
	this->dragBoxDrawNode->drawSolidRect(this->newBoxOrigin, this->newBoxDest, cocos2d::Color4F::ORANGE);
	this->dragBoxDrawNode->drawRect(this->newBoxOrigin, this->newBoxDest, cocos2d::Color4F::YELLOW);
}

void VisibilityScene::clearDrag()
{
	this->currentMode = MODE::IDLE;
	this->newBoxDest = cocos2d::Vec2::ZERO;
	this->newBoxOrigin = cocos2d::Vec2::ZERO;
	this->dragBoxDrawNode->clear();
}

void VisibilityScene::drawFreeformWall()
{
	this->freeformWallDrawNode->clear();

	auto size = this->freeformWallPoints.size();
	if (size < 1) return;

	for (unsigned int i = 0; i < size; i++)
	{
		this->freeformWallDrawNode->drawSolidCircle(this->freeformWallPoints.at(i), 7.5f, 360.0f, 20, cocos2d::Color4F::BLUE);

		if (i < size - 1)
		{
			this->freeformWallDrawNode->drawLine(this->freeformWallPoints.at(i), this->freeformWallPoints.at(i + 1), cocos2d::Color4F::YELLOW);
		}
	}

	if (size > 1)
	{
		this->freeformWallDrawNode->drawLine(this->freeformWallPoints.at(0), this->freeformWallPoints.at(size - 1), cocos2d::Color4F::YELLOW);
	}
}

void VisibilityScene::clearFreeform()
{
	this->freeformWallPoints.clear();
	this->freeformWallDrawNode->clear();
	this->currentMode = MODE::IDLE;
}

void VisibilityScene::drawWalls()
{
	this->wallDrawNode->clear();

	int index = 0;
	for (auto wall : this->walls)
	{
		if(wall.rectangle)
		{
			if (index == this->hoveringWallIndex)
			{
				this->wallDrawNode->drawSolidRect(wall.entities.at(3)->getComponent<ECS::Sprite>()->sprite->getPosition(), wall.entities.at(1)->getComponent<ECS::Sprite>()->sprite->getPosition(), cocos2d::Color4F::RED);
			}
			else
			{
				this->wallDrawNode->drawRect(wall.entities.at(3)->getComponent<ECS::Sprite>()->sprite->getPosition(), wall.entities.at(1)->getComponent<ECS::Sprite>()->sprite->getPosition(), cocos2d::Color4F::YELLOW);
			}
		}
		else
		{
			auto size = wall.points.size();
			for (unsigned int i = 0; i < size; i++)
			{
				if (i < size - 1)
				{
					this->wallDrawNode->drawLine(wall.points.at(i), wall.points.at(i + 1), cocos2d::Color4F::YELLOW);
				}
			}

			if (size > 1)
			{
				this->wallDrawNode->drawLine(wall.points.at(0), wall.points.at(size - 1), cocos2d::Color4F::YELLOW);
			}
		}

		index++;
	}
}

void VisibilityScene::drawRaycast()
{
	this->raycastDrawNode->clear();

	if (this->viewRaycast)
	{
		for (auto intersect : intersects)
		{
			auto color = cocos2d::Color4F::RED;
			//color.a = 0.5f;
			if (intersect.boundaryVisible || intersect.otherWallVisible)
			{
				this->raycastDrawNode->drawLine(this->mousePos, intersect.extendedVertex, color);
			}
			else
			{
				this->raycastDrawNode->drawLine(this->mousePos, intersect.vertex, color);
			}
		}
	}
}

void VisibilityScene::update(float delta)
{
	this->labelsNode->updateFPSLabel(delta);

	if (this->currentMode == MODE::IDLE)
	{
		// Cursor is light
		if (this->cursorLight)
		{
			if (this->displayBoundary.containsPoint(this->mousePos))
			{
				if (!this->isPointInWall(this->mousePos))
				{
					// Point is not in the wall
					this->findIntersectsWithRaycasts();
					this->drawTriangles();
				}
			}
		}
	}
	else
	{
		this->triangleDrawNode->clear();
	}
}

void VisibilityScene::initInputListeners()
{
	this->mouseInputListener = EventListenerMouse::create();
	this->mouseInputListener->onMouseMove = CC_CALLBACK_1(VisibilityScene::onMouseMove, this);
	this->mouseInputListener->onMouseDown = CC_CALLBACK_1(VisibilityScene::onMouseDown, this);
	this->mouseInputListener->onMouseUp = CC_CALLBACK_1(VisibilityScene::onMouseUp, this);
	this->mouseInputListener->onMouseScroll = CC_CALLBACK_1(VisibilityScene::onMouseScroll, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(this->mouseInputListener, this);

	this->keyInputListener = EventListenerKeyboard::create();
	this->keyInputListener->onKeyPressed = CC_CALLBACK_2(VisibilityScene::onKeyPressed, this);
	this->keyInputListener->onKeyReleased = CC_CALLBACK_2(VisibilityScene::onKeyReleased, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(this->keyInputListener, this);
}

void VisibilityScene::onMouseMove(cocos2d::Event* event) 
{
	auto mouseEvent = static_cast<EventMouse*>(event);
	//0 = left, 1 = right, 2 = middle
	int mouseButton = mouseEvent->getMouseButton();
	float x = mouseEvent->getCursorX();
	float y = mouseEvent->getCursorY();

	cocos2d::Vec2 point = cocos2d::Vec2(x, y);

	if (this->displayBoundary.containsPoint(point))
	{
		if (this->currentMode == MODE::IDLE)
		{
			int index = 0;
			bool hovered = false;
			for (auto wall : this->walls)
			{
				if (wall.rectangle)
				{
					if (wall.bb.containsPoint(point))
					{
						this->hoveringWallIndex = index;
						this->drawWalls();
						hovered = true;
						break;
					}
				}
				else
				{
					auto size = wall.points.size();
					if (size >= 3)
					{
						// use earclip scene for polygon
						//for (auto point : wall.points)
						//{
							//earClipping->finalVerticies.clear();
							//earClipping->finalVerticies.push_back(point);
							//earClipping->makeVerticies();
							//earClipping->currentSceneState = EarClippingScene::SCENE_STATE::ALGORITHM_STATE;
							//while (earClipping->currentSceneState != EarClippingScene::SCENE_STATE::FINISHED)
							//{
							//	earClipping->runEarClipping();
							//}
						//}

						std::list<cocos2d::Vec2> pointList(wall.points.begin(), wall.points.end());

						bool inPolygon = earClipping->isPointInPolygon(pointList, point);

						if (inPolygon)
						{
							this->hoveringWallIndex = index;
							this->drawWalls();
							hovered = true;
						}
					}
					else
					{
						// it's a line
						continue;
					}
				}

				index++;
			}

			if (!hovered)
			{
				if (this->hoveringWallIndex != -1)
				{
					this->hoveringWallIndex = -1;
					this->drawWalls();
				}
			}
		}
		else if (this->currentMode == MODE::DRAW_WALL_READY)
		{
			// Trying to make wall with dragging
			auto m = ECS::Manager::getInstance();
			auto boxCount = m->getAliveEntityCountInEntityPool("WALL_POINT");
			if (boxCount < maxWallPoints)
			{
				// still can make another box
				this->currentMode = MODE::DRAW_WALL_DRAG;		
				this->newBoxDest = point;
				this->drawDragBox();

			}
			else
			{
				// Can't make more boxes. return
				this->currentMode = MODE::IDLE;
				return;
			}
		}
		else if (this->currentMode == MODE::DRAW_WALL_DRAG)
		{
			if (!this->isPointInWall(point))
			{
				cocos2d::Vec2 size = point - this->newBoxOrigin;
				if (size.x > maxWallSegmentSize)
				{
					point.x = this->newBoxOrigin.x + maxWallSegmentSize;
				}
				else if (size.x < -maxWallSegmentSize)
				{
					point.x = this->newBoxOrigin.x - maxWallSegmentSize;
				}

				if (size.y > maxWallSegmentSize)
				{
					point.y = this->newBoxOrigin.y + maxWallSegmentSize;
				}
				else if (size.y < -maxWallSegmentSize)
				{
					point.y = this->newBoxOrigin.y - maxWallSegmentSize;
				}

				this->newBoxDest = point;

				this->drawDragBox();
			}
		}
	}

	this->mousePos = point;
}

void VisibilityScene::onMouseDown(cocos2d::Event* event) 
{
	auto mouseEvent = static_cast<EventMouse*>(event);
	//0 = left, 1 = right, 2 = middle
	int mouseButton = mouseEvent->getMouseButton();
	float x = mouseEvent->getCursorX();
	float y = mouseEvent->getCursorY();

	cocos2d::Vec2 point = cocos2d::Vec2(x, y);

	if (this->displayBoundary.containsPoint(point))
	{	
		if (this->currentMode == MODE::IDLE)
		{
			if (mouseButton == 0)
			{
				// Yet, we don't know if user is going to either drag or make point. 
				// So just put it in a ready state
				this->currentMode = MODE::DRAW_WALL_READY;
				this->newBoxOrigin = point;

			}
			else if(mouseButton == 1)
			{
				// Add light
				this->lightPositions.push_back(point);
			}
		}
		else if (this->currentMode == MODE::DRAW_WALL_POINT)
		{
			// And add point as first point.
			if (!this->isPointInWall(point))
			{
				this->freeformWallPoints.push_back(point);
				this->drawFreeformWall();
			}
		}
	}


}

void VisibilityScene::onMouseUp(cocos2d::Event* event) 
{
	auto mouseEvent = static_cast<EventMouse*>(event);
	//0 = left, 1 = right, 2 = middle
	int mouseButton = mouseEvent->getMouseButton();
	float x = mouseEvent->getCursorX();
	float y = mouseEvent->getCursorY();

	cocos2d::Vec2 point = cocos2d::Vec2(x, y);

	if (this->displayBoundary.containsPoint(point))
	{
		if (this->currentMode == MODE::IDLE)
		{
			// Do nothing?
		}
		else if (this->currentMode == MODE::DRAW_WALL_READY)
		{
			// Draw Freeform wall 
			this->currentMode = MODE::DRAW_WALL_POINT;
			// And add point as first point.
			this->freeformWallPoints.clear();
			this->freeformWallPoints.push_back(point);
			this->drawFreeformWall();
		}
		else if (this->currentMode == MODE::DRAW_WALL_DRAG)
		{
			// End drawing				
			if (this->newBoxOrigin != this->newBoxDest)
			{
				// Make new box
				this->createNewRectWall();
				cocos2d::log("Creating new box origin (%f, %f), size (%f, %f)", this->newBoxOrigin.x, this->newBoxOrigin.y, this->newBoxDest.x, this->newBoxDest.y);

				this->clearDrag();
			}
		}
	}
	else
	{
		if (this->currentMode == MODE::DRAW_WALL_DRAG)
		{
			// Cancle draw
			this->currentMode = MODE::IDLE;
			this->newBoxDest = cocos2d::Vec2::ZERO;
			this->newBoxOrigin = cocos2d::Vec2::ZERO;
			this->dragBoxDrawNode->clear();
		}
	}
}

void VisibilityScene::onMouseScroll(cocos2d::Event* event) 
{
	//auto mouseEvent = static_cast<EventMouse*>(event);
	//float x = mouseEvent->getScrollX();
	//float y = mouseEvent->getScrollY();
}

void VisibilityScene::onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) 
{
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		if (this->currentMode == MODE::DRAW_WALL_POINT)
		{
			this->clearFreeform();
		}
		else if (this->currentMode == MODE::DRAW_WALL_DRAG || this->currentMode == MODE::DRAW_WALL_READY)
		{
			this->clearDrag();
		}
		else if (this->currentMode == MODE::IDLE)
		{
			// Terminate
			cocos2d::Director::getInstance()->replaceScene(cocos2d::TransitionFade::create(0.5f, MainScene::create(), cocos2d::Color3B::BLACK));
		}
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_ENTER)
	{
		if (this->currentMode == MODE::DRAW_WALL_POINT)
		{
			if (this->freeformWallPoints.size() < 1)
			{
				this->clearFreeform();
			}
			else
			{
				this->createNewFreeformWall();
				this->clearFreeform();
			}
		}
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_L)
	{
		//this->viewRaycast = !this->viewRaycast;
		this->cursorLight = !this->cursorLight;
	}


	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_1)
	{
		// debug.
		// Does getIntersection works for both edge?
		Segment* seg = new Segment;

		seg->p1 = cocos2d::Vec2(68, 72);
		seg->p2 = cocos2d::Vec2(218, 185);

		auto lightPos = cocos2d::Vec2(436, 54);

		float angle1 = atan2(seg->p1.y - lightPos.y, seg->p1.x - lightPos.x);
		float angle2 = atan2(seg->p2.y - lightPos.y, seg->p2.x - lightPos.x);

		// Get direction of ray
		float dx1 = cosf(angle1);
		float dy1 = sinf(angle1);

		// Generate raycast vec2 points
		auto rayStart1 = lightPos;
		auto rayEnd1 = cocos2d::Vec2(rayStart1.x + dx1, rayStart1.y + dy1);

		// Get direction of ray
		float dx2 = cosf(angle2);
		float dy2 = sinf(angle2);

		// Generate raycast vec2 points
		auto rayStart2 = lightPos;
		auto rayEnd2 = cocos2d::Vec2(rayStart2.x + dx2, rayStart2.y + dy2);

		float dx3 = cosf(angle2 + 0.00001f);
		float dy3 = sinf(angle2 + 0.00001f);
		float dx4 = cosf(angle2 - 0.00001f);
		float dy4 = sinf(angle2 - 0.00001f);

		auto rayStart3 = lightPos;
		auto rayEnd3 = cocos2d::Vec2(rayStart3.x + dx3, rayStart3.y + dy3);
		auto rayStart4 = lightPos;
		auto rayEnd4 = cocos2d::Vec2(rayStart4.x + dx4, rayStart4.y + dy4);

		cocos2d::Vec2 intersection1;
		float dist1 = this->getIntersectingPoint(rayStart1, rayEnd1, seg, intersection1);
		cocos2d::Vec2 intersection2;
		float dist2 = this->getIntersectingPoint(rayStart2, rayEnd2, seg, intersection2);
		cocos2d::Vec2 intersection3;
		float dist3 = this->getIntersectingPoint(rayStart3, rayEnd3, seg, intersection3);
		cocos2d::Vec2 intersection4;
		float dist4 = this->getIntersectingPoint(rayStart4, rayEnd4, seg, intersection4);

		if (dist1 > 0)
		{
			cocos2d::log("P1 intersects segment");
			cocos2d::log("Intersection 1 = (%f, %f)", intersection1.x, intersection1.y);
			cocos2d::log("Ray to p1 = %f", rayStart1.distance(seg->p1));
			cocos2d::log("dist = %f", dist1);
		}
		if (dist2 > 0)
		{
			cocos2d::log("P2 intersects segment");
			cocos2d::log("Intersection 2 = (%f, %f)", intersection2.x, intersection2.y);
			cocos2d::log("Ray to p2 = %f", rayStart2.distance(seg->p2));
			cocos2d::log("dist = %f", dist2);
		}
		if (dist3 > 0)
		{
			cocos2d::log("P3 intersects segment");
			cocos2d::log("Intersection 3 = (%f, %f)", intersection3.x, intersection3.y);
			cocos2d::log("Ray to p3 = %f", rayStart3.distance(seg->p2));
			cocos2d::log("dist = %f", dist3);
		}
		if (dist4 > 0)
		{
			cocos2d::log("P4 intersects segment");
			cocos2d::log("Intersection 4 = (%f, %f)", intersection4.x, intersection4.y);
			cocos2d::log("Ray to p4 = %f", rayStart4.distance(seg->p2));
			cocos2d::log("dist = %f", dist4);
		}

		delete seg;
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_2)
	{
		// Invert case of above case
		Segment* seg = new Segment;

		seg->p1 = cocos2d::Vec2(218, 185);
		seg->p2 = cocos2d::Vec2(68, 72);

		auto lightPos = cocos2d::Vec2(436, 54);

		float angle1 = atan2(seg->p1.y - lightPos.y, seg->p1.x - lightPos.x);
		float angle2 = atan2(seg->p2.y - lightPos.y, seg->p2.x - lightPos.x);

		// Get direction of ray
		float dx1 = cosf(angle1);
		float dy1 = sinf(angle1);

		// Generate raycast vec2 points
		auto rayStart1 = lightPos;
		auto rayEnd1 = cocos2d::Vec2(rayStart1.x + dx1, rayStart1.y + dy1);

		// Get direction of ray
		float dx2 = cosf(angle2);
		float dy2 = sinf(angle2);

		// Generate raycast vec2 points
		auto rayStart2 = lightPos;
		auto rayEnd2 = cocos2d::Vec2(rayStart2.x + dx2, rayStart2.y + dy2);

		cocos2d::Vec2 intersection1;
		float dist1 = this->getIntersectingPoint(rayStart1, rayEnd1, seg, intersection1);
		cocos2d::Vec2 intersection2;
		float dist2 = this->getIntersectingPoint(rayStart2, rayEnd2, seg, intersection2);

		if (dist1 > 0)
		{
			cocos2d::log("P1 intersects segment");
			cocos2d::log("Ray to p1 = %f", rayStart1.distance(seg->p1));
			cocos2d::log("dist = %f", dist1);
		}
		if (dist2 > 0)
		{
			cocos2d::log("P2 intersects segment");
			cocos2d::log("Ray to p2 = %f", rayStart2.distance(seg->p2));
			cocos2d::log("dist = %f", dist2);
		}

		delete seg;
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_3)
	{
		// debug.
		// Does getIntersection works for both edge?
		Segment* seg = new Segment;

		seg->p1 = cocos2d::Vec2(68, 72);
		seg->p2 = cocos2d::Vec2(218, 185);

		auto lightPos = cocos2d::Vec2(136, 254);

		float angle1 = atan2(seg->p1.y - lightPos.y, seg->p1.x - lightPos.x);
		float angle2 = atan2(seg->p2.y - lightPos.y, seg->p2.x - lightPos.x);

		// Get direction of ray
		float dx1 = cosf(angle1);
		float dy1 = sinf(angle1);

		// Generate raycast vec2 points
		auto rayStart1 = lightPos;
		auto rayEnd1 = cocos2d::Vec2(rayStart1.x + dx1, rayStart1.y + dy1);

		// Get direction of ray
		float dx2 = cosf(angle2);
		float dy2 = sinf(angle2);

		// Generate raycast vec2 points
		auto rayStart2 = lightPos;
		auto rayEnd2 = cocos2d::Vec2(rayStart2.x + dx2, rayStart2.y + dy2);

		cocos2d::Vec2 intersection1;
		float dist1 = this->getIntersectingPoint(rayStart1, rayEnd1, seg, intersection1);
		cocos2d::Vec2 intersection2;
		float dist2 = this->getIntersectingPoint(rayStart2, rayEnd2, seg, intersection2);

		if (dist1 > 0)
		{
			cocos2d::log("P1 intersects segment");
			cocos2d::log("Ray to p1 = %f", rayStart1.distance(seg->p1));
			cocos2d::log("dist = %f", dist1);
		}
		if (dist2 > 0)
		{
			cocos2d::log("P2 intersects segment");
			cocos2d::log("Ray to p2 = %f", rayStart2.distance(seg->p2));
			cocos2d::log("dist = %f", dist2);
		}

		delete seg;
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_4)
	{
		// debug.
		// Does getIntersection works for both edge?
		auto p3 = cocos2d::Vec2(68, 72);
		auto p4 = cocos2d::Vec2(218, 185);

		auto lightPos = cocos2d::Vec2(136, 254);

		float angle1 = atan2(p3.y - lightPos.y, p3.x - lightPos.x);
		float angle2 = atan2(p4.y - lightPos.y, p4.x - lightPos.x);

		// Get direction of ray
		float dx1 = cosf(angle1);
		float dy1 = sinf(angle1);

		// Generate raycast vec2 points
		auto rayStart1 = lightPos;
		auto rayEnd1 = cocos2d::Vec2(rayStart1.x + dx1, rayStart1.y + dy1);

		// Get direction of ray
		float dx2 = cosf(angle2);
		float dy2 = sinf(angle2);

		// Generate raycast vec2 points
		auto rayStart2 = lightPos;
		auto rayEnd2 = cocos2d::Vec2(rayStart2.x + dx2, rayStart2.y + dy2);

		cocos2d::Vec2 intersection1 = this->getIntersectingPoint(rayStart1, rayEnd1, p3, p4);
		cocos2d::Vec2 intersection2 = this->getIntersectingPoint(rayStart2, rayEnd2, p3, p4);

		cocos2d::log("Intersection 1 = (%f, %f)", intersection1.x, intersection1.y);
		cocos2d::log("Intersection 2 = (%f, %f)", intersection2.x, intersection2.y);

		if (intersection1 == p3)
		{
			cocos2d::log("Intersection 1 correct");
		}
		
		if (intersection2 == p4)
		{
			cocos2d::log("Intersection 2 correct");
		}
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_5)
	{
		// debug.
		// Does getIntersection works for both edge?
		auto p3 = cocos2d::Vec2(68, 72);
		auto p4 = cocos2d::Vec2(218, 185);

		auto lightPos = cocos2d::Vec2(136, 254);

		float angle1 = atan2(p3.y - lightPos.y, p3.x - lightPos.x);
		float angle2 = atan2(p4.y - lightPos.y, p4.x - lightPos.x);

		// Get direction of ray
		float dx1 = cosf(-angle1);
		float dy1 = sinf(-angle1);

		// Generate raycast vec2 points
		auto rayStart1 = lightPos;
		auto rayEnd1 = cocos2d::Vec2(rayStart1.x + dx1, rayStart1.y + dy1);

		// Get direction of ray
		float dx2 = cosf(-angle2);
		float dy2 = sinf(-angle2);

		// Generate raycast vec2 points
		auto rayStart2 = lightPos;
		auto rayEnd2 = cocos2d::Vec2(rayStart2.x + dx2, rayStart2.y + dy2);

		cocos2d::Vec2 intersection1 = this->getIntersectingPoint(rayStart1, rayEnd1, p3, p4);
		cocos2d::Vec2 intersection2 = this->getIntersectingPoint(rayStart2, rayEnd2, p3, p4);

		cocos2d::log("Intersection 1 = (%f, %f)", intersection1.x, intersection1.y);
		cocos2d::log("Intersection 2 = (%f, %f)", intersection2.x, intersection2.y);

		if (intersection1 == p3)
		{
			cocos2d::log("Intersection 1 correct");
		}

		if (intersection2 == p4)
		{
			cocos2d::log("Intersection 2 correct");
		}
	}
}

void VisibilityScene::onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) 
{

}

void VisibilityScene::releaseInputListeners()
{
	if(this->mouseInputListener != nullptr)
		_eventDispatcher->removeEventListener(this->mouseInputListener);
	if(this->keyInputListener != nullptr)
		_eventDispatcher->removeEventListener(this->keyInputListener);
}

void VisibilityScene::onExit()
{
	cocos2d::Scene::onExit();

	releaseInputListeners(); 

	ECS::Manager::deleteInstance();

	for (auto segment : this->wallSegments)
	{
		delete segment;
	}

	this->earClipping->release();
}
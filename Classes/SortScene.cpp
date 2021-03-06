#include "SortScene.h"
#include <algorithm>
#include "MainScene.h"

USING_NS_CC;

SortScene* SortScene::createScene()
{
	SortScene* newSortScene = SortScene::create();
	return newSortScene;
}

bool SortScene::init()
{
	if (!cocos2d::Scene::init())
	{
		return false;
	}

	// Uncomment this to activate update(float) function
	this->scheduleUpdate();

	hoveringBarIndex = -1;

	barInfoLabel = cocos2d::Label::createWithTTF("", "fonts/Rubik-Medium.ttf", 20);
	barInfoLabel->setPosition(cocos2d::Vec2(360.0f, 16.0f));
	this->addChild(barInfoLabel);

	paused = false;

	const float MAX_VALUE_SIZE_F = static_cast<float>(MAX_VALUE_SIZE);
	barScaleX = 650.0f / MAX_VALUE_SIZE_F;
	barScaleYMul = MAX_VALUE_SIZE_F;

	// init display boundary box node which draws outer line of simulation display box
	this->displayBoundaryBoxNode = DisplayBoundaryBoxNode::createNode();
	this->displayBoundaryBoxNode->setPosition(cocos2d::Vec2::ZERO);
	this->displayBoundaryBoxNode->displayBoundary = cocos2d::Rect(0, 0, 650, 650);
	this->displayBoundaryBoxNode->drawDisplayBoundaryBox();
	this->displayBoundaryBoxNode->retain();
	this->displayBoundaryBoxNode->drawNode->setLocalZOrder(static_cast<int>(Z_ORDER::BOUNDARY_WALL));
	this->addChild(this->displayBoundaryBoxNode, Z_ORDER::BOUNDARY_WALL);

	for (int i = 0; i < 100; i++)
	{
		bars.push_back(cocos2d::Sprite::createWithSpriteFrameName("dot.png"));
		bars.back()->setScaleX(10.0f);
		bars.back()->setAnchorPoint(cocos2d::Vec2(0, 0));
		bars.back()->setPosition(cocos2d::Vec2(35.0f + (static_cast<float>(i) * barScaleX), 36.0f));
		bars.back()->setLocalZOrder(Z_ORDER::BAR);
		this->addChild(bars.back());

		if (i < MAX_VALUE_SIZE)
		{
			bars.back()->setVisible(true);
		}
		else
		{
			bars.back()->setVisible(false);
		}
	}

	resetValues();

	sortMode = SORT_MODE::NONE;

	checkSpeed = 0.07f;
	checkIndex = 0;

	mergeSortRoot = nullptr;
	quickSortRoot = nullptr;

	auto winSize = cocos2d::Director::getInstance()->getVisibleSize();

	// Init labels node
	this->labelsNode = LabelsNode::createNode();
	this->labelsNode->setSharedLabelPosition(LabelsNode::SHARED_LABEL_POS_TYPE::QUADTREE_SCENE);
	this->addChild(this->labelsNode);

	// Starting pos
	float labelX = winSize.height - 10.0f;
	float labelY = winSize.height - 45.0f;

	// Set title
	this->labelsNode->initTitleStr("Sort", cocos2d::Vec2(labelX, labelY));

	labelY -= 50.0f;

	// Init custom labels
	this->labelsNode->customLabelStartPos = cocos2d::Vec2(labelX, labelY);

	// Set size
	const int customLabelSize = 25;

	this->labelsNode->addLabel(LabelsNode::TYPE::CUSTOM, "Status: Idle", customLabelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::CUSTOM, "Current sort: None", customLabelSize);

	// Calculate next label block y
	const float customLastY = this->labelsNode->customLabels.back()->getBoundingBox().getMinY();
	const float blockGap = 22.0f;

	this->labelsNode->keyboardUsageLabelStartPos = cocos2d::Vec2(labelX, customLastY - blockGap);

	const int headerSize = 25;
	const int labelSize = 20;

	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "Keys (Green = enabled)", headerSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "Space: Toggle update", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "Enter: Step", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "R: Reset", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "1: Selection sort", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "2: Insertion sort", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "3: Merge", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "4: Bubble", labelSize);
	this->labelsNode->addLabel(LabelsNode::TYPE::KEYBOARD, "5: Quick", labelSize);

	float sliderY = this->labelsNode->keyboardUsageLabels.back()->getBoundingBox().getMinY();

	this->barCountSliderNode = SliderLabelNode::createNode();
	this->barCountSliderNode->sliderStartPos = cocos2d::Vec2(labelX, sliderY - blockGap);
	this->barCountSliderNode->addSlider("Array size (10 ~ 100)", "Slider", 50, CC_CALLBACK_1(SortScene::onArraySizeSliderClick, this));
	this->barCountSliderNode->sliderLabels.back().slider->setMaxPercent(90);
	this->barCountSliderNode->sliderLabels.back().slider->setPercent(55);
	this->addChild(this->barCountSliderNode);

	sliderY -= 50.0f;
	
	this->simulationSpeedModifier = 1.0f;

	this->sliderLabelNode = SliderLabelNode::createNode();
	this->sliderLabelNode->sliderStartPos = cocos2d::Vec2(labelX, sliderY - blockGap);
	this->sliderLabelNode->addSlider("Simulation Speed", "Slider", 50, CC_CALLBACK_1(SortScene::onSliderClick, this));
	this->addChild(this->sliderLabelNode);

	return true;
}

void SortScene::onEnter()
{
	cocos2d::Scene::onEnter();
	// Uncomment this to enable mouse and keyboard event listeners
	initInputListeners();
}

void SortScene::update(float delta)
{
	this->labelsNode->updateFPSLabel(delta);

	float dt = delta * simulationSpeedModifier;

	switch (sortMode)
	{
	case SortScene::SORT_MODE::SELECTION:
	{
		updateSelectionSort(delta);
	}
		break;
	case SortScene::SORT_MODE::INSERTION:
	{
		updateInsertionSort(delta);
	}
		break;
	case SortScene::SORT_MODE::MERGE:
	{
		updateMergeSort(delta);
	}
		break;
	case SortScene::SORT_MODE::BUBBLE:
	{
		updateBubbleSort(delta);
	}
		break;
	case SortScene::SORT_MODE::QUICK:
	{
		updateQuickSort(delta);
	}
		break;
	case SortScene::SORT_MODE::NONE:
	default:
		break;
	}
}

void SortScene::initInputListeners()
{
	this->mouseInputListener = EventListenerMouse::create();
	this->mouseInputListener->onMouseMove = CC_CALLBACK_1(SortScene::onMouseMove, this);
	this->mouseInputListener->onMouseDown = CC_CALLBACK_1(SortScene::onMouseDown, this);
	this->mouseInputListener->onMouseUp = CC_CALLBACK_1(SortScene::onMouseUp, this);
	this->mouseInputListener->onMouseScroll = CC_CALLBACK_1(SortScene::onMouseScroll, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(this->mouseInputListener, this);

	this->keyInputListener = EventListenerKeyboard::create();
	this->keyInputListener->onKeyPressed = CC_CALLBACK_2(SortScene::onKeyPressed, this);
	this->keyInputListener->onKeyReleased = CC_CALLBACK_2(SortScene::onKeyReleased, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(this->keyInputListener, this);
}

void SortScene::onMouseMove(cocos2d::Event* event) 
{
	auto mouseEvent = static_cast<EventMouse*>(event);
	float x = mouseEvent->getCursorX();
	float y = mouseEvent->getCursorY();

	checkMouseOver(cocos2d::Vec2(x, y));
}

void SortScene::onMouseDown(cocos2d::Event* event) 
{
	//auto mouseEvent = static_cast<EventMouse*>(event);
	//0 = left, 1 = right, 2 = middle
	//int mouseButton = mouseEvent->getMouseButton();
	//float x = mouseEvent->getCursorX();
	//float y = mouseEvent->getCursorY();
}

void SortScene::onMouseUp(cocos2d::Event* event) 
{
	//auto mouseEvent = static_cast<EventMouse*>(event);
	//0 = left, 1 = right, 2 = middle
	//int mouseButton = mouseEvent->getMouseButton();
	//float x = mouseEvent->getCursorX();
	//float y = mouseEvent->getCursorY();
}

void SortScene::onMouseScroll(cocos2d::Event* event) 
{
	//auto mouseEvent = static_cast<EventMouse*>(event);
	//float x = mouseEvent->getScrollX();
	//float y = mouseEvent->getScrollY();
}

void SortScene::onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) 
{
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		cocos2d::Director::getInstance()->replaceScene(cocos2d::TransitionFade::create(0.5f, MainScene::create(), cocos2d::Color3B::BLACK));

	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_SPACE)
	{
		paused = !paused;
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::PAUSE), paused ? cocos2d::Color3B::GREEN : cocos2d::Color3B::WHITE);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_ENTER)
	{
		if (paused)
		{
			switch (sortMode)
			{
			case SortScene::SORT_MODE::SELECTION:
				searchElapsedTime = 0;
				if (selectionSortState == SELECTION_SORT_STATE::SEARCHING_MIN_VALUE)
				{
					stepSelectionSort();
				}
				break;
			case SortScene::SORT_MODE::INSERTION:
				if (insertionSortState == INSERTION_SORT_STATE::SELECTING_NEXT)
				{
					stepInsertionSort();
				}
				break;
			case SortScene::SORT_MODE::MERGE:
			{
				if (mergeSortState == MERGE_SORT_STATE::SEARCH)
				{
					stepMergeSort();
				}
			}
				break;
			case SortScene::SORT_MODE::BUBBLE:
			{
				if (bubbleSortState == BUBBLE_SORT_STATE::NEXT)
				{
					stepBubbleSort();
				}
			}
				break;
			case SortScene::SORT_MODE::QUICK:
			{
				if (quickSortState == QUICK_SORT_STATE::PARTITION)
				{
					stepQuickSort();
				}
			}
				break;
			default:
			case SortScene::SORT_MODE::NONE:
				break;
			}
			this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::STEP), cocos2d::Color3B::WHITE);
		}
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_R)
	{
		clearPrevSortModeLabelColor();
		reset();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::RESET), cocos2d::Color3B::WHITE);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(true);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_1)
	{
		// selection sort
		clearPrevSortModeLabelColor();
		initSelectionSort();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_1),cocos2d::Color3B::GREEN);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(false);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_2)
	{
		// insertion sort
		clearPrevSortModeLabelColor();
		initInsertionSort();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_2), cocos2d::Color3B::GREEN);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(false);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_3)
	{
		// merge sort
		clearPrevSortModeLabelColor();
		initMergeSort();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_3), cocos2d::Color3B::GREEN);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(false);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_4)
	{
		// bubble sort
		clearPrevSortModeLabelColor();
		initBubbleSort();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_4), cocos2d::Color3B::GREEN);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(false);
	}
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_5)
	{
		// quick sort
		clearPrevSortModeLabelColor();
		initQuickSort();
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_5), cocos2d::Color3B::GREEN);
		this->barCountSliderNode->sliderLabels.back().slider->setEnabled(false);
	}
}

void SortScene::onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) 
{

}

void SortScene::releaseInputListeners()
{
	if(this->mouseInputListener != nullptr)
		_eventDispatcher->removeEventListener(this->mouseInputListener);
	if(this->keyInputListener != nullptr)
		_eventDispatcher->removeEventListener(this->keyInputListener);
}

void SortScene::resetValues()
{
	values.clear();

	for (int i = 1; i <= MAX_VALUE_SIZE; i++)
	{
		values.push_back(i);
	}

	for (int i = 0; i < MAX_VALUE_SIZE; i++)
	{
		bars.at(i)->setScaleY(static_cast<float>(values.at(i)) * 650.0f / static_cast<float>(MAX_VALUE_SIZE));
	}
}

void SortScene::randomizeValues()
{
	values.clear();

	for (int i = 1; i <= MAX_VALUE_SIZE; i++)
	{
		values.push_back(i);
	}

	std::random_shuffle(values.begin(), values.end());
	
	for (int i = 0; i < MAX_VALUE_SIZE; i++)
	{
		bars.at(i)->stopAllActions();
		bars.at(i)->setScaleY(static_cast<float>(values.at(i)) * 650.0f / static_cast<float>(MAX_VALUE_SIZE));
	}
}

void SortScene::reset()
{
	sortMode = SORT_MODE::NONE;
	resetValues();

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Idle");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: None");

	resetBar();
}

void SortScene::resetBar()
{
	for (auto bar : bars)
	{
		bar->setColor(cocos2d::Color3B::WHITE);
		bar->stopAllActions();
	}
}

void SortScene::clearPrevSortModeLabelColor()
{
	switch (sortMode)
	{
	case SortScene::SORT_MODE::SELECTION:
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_1), cocos2d::Color3B::WHITE, false);
		break;
	case SortScene::SORT_MODE::INSERTION:
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_2), cocos2d::Color3B::WHITE, false);
		break;
	case SortScene::SORT_MODE::MERGE:
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_3), cocos2d::Color3B::WHITE, false);
		break;
	case SortScene::SORT_MODE::BUBBLE:
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_4), cocos2d::Color3B::WHITE, false);
		break;
	case SortScene::SORT_MODE::QUICK:
		this->labelsNode->setColor(LabelsNode::TYPE::KEYBOARD, static_cast<int>(USAGE_KEY::KEY_5), cocos2d::Color3B::WHITE, false);
		break;
	case SortScene::SORT_MODE::NONE:
	default:
		break;
	}
}

void SortScene::initSelectionSort()
{
	randomizeValues();
	resetBar();
	sortMode = SORT_MODE::SELECTION;

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Sorting");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: Selection Sort");

	minSearchIndex = 0;
	minSelectedIndex = -1;
	sortedIndex = -1;
	searchSpeed = 0.04f;
	searchElapsedTime = 0;
	selectionSortState = SELECTION_SORT_STATE::NONE;

	checkIndex = 0;
	checkElapsedTime = 0.0f;
}

void SortScene::updateSelectionSort(const float delta)
{
	if (selectionSortState == SELECTION_SORT_STATE::NONE)
	{
		selectionSortState = SELECTION_SORT_STATE::SEARCHING_MIN_VALUE;
		//bars.at(0)->setColor(cocos2d::Color3B::RED);
	}
	else if (selectionSortState == SELECTION_SORT_STATE::SEARCHING_MIN_VALUE)
	{
		if (paused) return;

		searchElapsedTime += (delta * simulationSpeedModifier * 2.0f);
		if (searchElapsedTime >= searchSpeed * 0.5f)
		{
			//bars.at(minSearchIndex)
			searchElapsedTime -= (searchSpeed * 0.5f);
			stepSelectionSort();
		}
	}
	else if (selectionSortState == SELECTION_SORT_STATE::SWAP)
	{
		searchElapsedTime += (delta * simulationSpeedModifier * 2.0f);
		if (searchElapsedTime >= searchSpeed * 0.5f)
		{
			searchElapsedTime = 0;
			selectionSortState = SELECTION_SORT_STATE::SEARCHING_MIN_VALUE;

			int siValue = values.at(sortedIndex);
			int minValue = values.at(minSelectedIndex);

			values.at(sortedIndex) = minValue;
			values.at(minSelectedIndex) = siValue;

			bars.at(sortedIndex)->stopAllActions();
			bars.at(minSelectedIndex)->stopAllActions();

			bars.at(minSelectedIndex)->setScaleY((static_cast<float>(siValue) * 650.0f / barScaleYMul));
			bars.at(minSelectedIndex)->setColor(cocos2d::Color3B::WHITE);
			bars.at(sortedIndex)->setScaleY((static_cast<float>(minValue) * 650.0f / barScaleYMul));
			bars.at(sortedIndex)->setColor(cocos2d::Color3B::YELLOW);

			if (hoveringBarIndex >= 0)
			{
				if (sortedIndex == hoveringBarIndex)
				{
					barInfoLabel->setString("Index: " + std::to_string(sortedIndex) + ", value = " + std::to_string(values.at(sortedIndex)) + ", State: Sorted");
				}
			}

			minSearchIndex = 0;
			minSelectedIndex = -1;

			minSearchIndex = sortedIndex + 1;

			searchElapsedTime = 0;
			
		}
	}
	else if (selectionSortState == SELECTION_SORT_STATE::CHECK)
	{
		checkSort(delta);
	}
}

void SortScene::stepSelectionSort()
{
	if (minSearchIndex < MAX_VALUE_SIZE)
	{
		// change previous searching bar to white. if it was min bar, mark red
		if (minSearchIndex >(sortedIndex + 1))
		{
			if ((minSearchIndex - 1) != minSelectedIndex)
			{
				bars.at(minSearchIndex - 1)->setColor(cocos2d::Color3B::WHITE);
			}
			else
			{
				bars.at(minSearchIndex - 1)->setColor(cocos2d::Color3B::RED);
			}
		}

		// set current bar to blue
		bars.at(minSearchIndex)->setColor(cocos2d::Color3B::BLUE);

		// check if minimum value was found this iteration
		if (minSelectedIndex == -1)
		{
			// not found. set as after sorted Index
			minSelectedIndex = sortedIndex + 1;
			// mark as red only if it's not blue
			if (minSelectedIndex != minSearchIndex)
			{
				bars.at(minSelectedIndex)->setColor(cocos2d::Color3B::RED);
			}
		}
		else
		{
			// there is min value. check with new value
			if (values.at(minSelectedIndex) > values.at(minSearchIndex))
			{
				bars.at(minSelectedIndex)->setColor(cocos2d::Color3B::WHITE);
				minSelectedIndex = minSearchIndex;
				bars.at(minSelectedIndex)->setColor(cocos2d::Color3B::RED);
			}
		}

		minSearchIndex++;
	}
	else
	{
		sortedIndex++;
		if (sortedIndex == MAX_VALUE_SIZE)
		{
			selectionSortState = SELECTION_SORT_STATE::CHECK;
			this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Checking");
		}
		else
		{
			int siValue = values.at(sortedIndex);
			int minValue = values.at(minSelectedIndex);
			
			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
			bars.at(sortedIndex)->runAction(cocos2d::ScaleTo::create(searchSpeed * ssmFlip, barScaleX, (static_cast<float>(minValue) * 650.0f / barScaleYMul), 1.0f));
			bars.at(sortedIndex)->runAction(cocos2d::TintTo::create(searchSpeed * ssmFlip, cocos2d::Color3B::RED));
			bars.at(minSelectedIndex)->runAction(cocos2d::ScaleTo::create(searchSpeed * ssmFlip, barScaleX, (static_cast<float>(siValue) * 650.0f / barScaleYMul), 1.0f));
			bars.at(minSelectedIndex)->runAction(cocos2d::TintTo::create(searchSpeed * ssmFlip, cocos2d::Color3B::WHITE));

			bars.at(minSearchIndex - 1)->setColor(cocos2d::Color3B::WHITE);
			selectionSortState = SELECTION_SORT_STATE::SWAP;

			searchElapsedTime = 0;
		}
	}
}

void SortScene::initInsertionSort()
{
	randomizeValues();
	resetBar();
	sortMode = SORT_MODE::INSERTION;

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Sorting");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: Insertion Sort");

	curInsertionSortIndex = -1;
	swappingIndex = 0;
	animSpeed = 0.06f;
	insertionSortState = INSERTION_SORT_STATE::NONE;

	checkIndex = 0;
	checkElapsedTime = 0.0f;
}

void SortScene::updateInsertionSort(const float delta)
{
	if (insertionSortState == INSERTION_SORT_STATE::NONE)
	{
		if (curInsertionSortIndex == -1)
		{
			curInsertionSortIndex = 0;
			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
			bars.at(curInsertionSortIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::BLUE));
		}

		if (bars.at(curInsertionSortIndex)->getNumberOfRunningActions() == 0)
		{
			insertionSortState = INSERTION_SORT_STATE::SWAP;
		}
	}
	else if (insertionSortState == INSERTION_SORT_STATE::SELECTING_NEXT)
	{
		if (paused) return;

		stepInsertionSort();
	}
	else if (insertionSortState == INSERTION_SORT_STATE::SWAP)
	{
		if (curInsertionSortIndex == 0)
		{
			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
			bars.at(curInsertionSortIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::YELLOW));
		}
		else
		{
			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

			if (swappingIndex >= 0)
			{
				int curVal = values.at(curInsertionSortIndex);

				for (int i = curInsertionSortIndex; i >= swappingIndex; i--)
				{
					if (i == swappingIndex)
					{
						bars.at(i)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(curVal) * 650.0f / barScaleYMul), 1.0f));
						values.at(i) = curVal;
					}
					else
					{
						bars.at(i)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(values.at(i - 1)) * 650.0f / barScaleYMul), 1.0f));
						bars.at(i)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::YELLOW));
						values.at(i) = values.at(i - 1);
					}

					if (hoveringBarIndex >= 0)
					{
						if (i == hoveringBarIndex)
						{
							barInfoLabel->setString("Index: " + std::to_string(i) + ", value = " + std::to_string(values.at(i)) + ", State: Sorted");
						}
					}
				}
			}
			else
			{
				float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
				bars.at(curInsertionSortIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::YELLOW));

				if (hoveringBarIndex >= 0)
				{
					if (curInsertionSortIndex == hoveringBarIndex)
					{
						barInfoLabel->setString("Index: " + std::to_string(curInsertionSortIndex) + ", value = " + std::to_string(values.at(curInsertionSortIndex)) + ", State: Sorted");
					}
				}
			}
		}

		insertionSortState = INSERTION_SORT_STATE::SWAPPING;
	}
	else if (insertionSortState == INSERTION_SORT_STATE::SWAPPING)
	{
		if (bars.at(curInsertionSortIndex)->getNumberOfRunningActions() == 0)
		{
			insertionSortState = INSERTION_SORT_STATE::SELECTING_NEXT;
			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
			curInsertionSortIndex++;

			if (curInsertionSortIndex >= MAX_VALUE_SIZE)
			{
				insertionSortState = INSERTION_SORT_STATE::CHECK;
				this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Checking");
				return;
			}

			bars.at(curInsertionSortIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::BLUE));
		}
	}
	else if (insertionSortState == INSERTION_SORT_STATE::CHECK)
	{
		checkSort(delta);
	}
}

void SortScene::stepInsertionSort()
{
	if (bars.at(curInsertionSortIndex)->getNumberOfRunningActions() == 0)
	{
		insertionSortState = INSERTION_SORT_STATE::SWAP;

		swappingIndex = -1;
		int curVal = values.at(curInsertionSortIndex);
		for (int i = curInsertionSortIndex - 1; i >= 0; i--)
		{
			if (curVal < values.at(i))
			{
				swappingIndex = i;
			}
			else
			{
				break;
			}
		}
	}
}

void SortScene::initMergeSort()
{
	randomizeValues();
	resetBar();
	sortMode = SORT_MODE::MERGE;

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Sorting");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: Merge Sort");

	if (mergeSortRoot) delete mergeSortRoot;

	mergeSortRoot = new MergeElem();
	mergeSortRoot->startIndex = 0;
	mergeSortRoot->endIndex = MAX_VALUE_SIZE - 1;

	mergeSortDelay = 0.1f;
	mergeSortElapsedTime = 0;
	animSpeed = 0.06f;

	checkIndex = 0;
	buildMergeElemTree(mergeSortRoot, 0, MAX_VALUE_SIZE - 1, MAX_VALUE_SIZE);

	mergeSortState = MERGE_SORT_STATE::NONE;
}

void SortScene::updateMergeSort(const float delta)
{
	if (mergeSortState == MERGE_SORT_STATE::NONE)
	{
		mergeSortState = MERGE_SORT_STATE::SEARCH;
		mergeSortElapsedTime = mergeSortDelay;
	}
	else if(mergeSortState == MERGE_SORT_STATE::SEARCH)
	{
		if (paused) return;

		mergeSortElapsedTime += (delta * simulationSpeedModifier);
		if (mergeSortElapsedTime >= mergeSortDelay)
		{
			stepMergeSort();
		}
	}
	else if (mergeSortState == MERGE_SORT_STATE::SORT)
	{
		mergeSortElapsedTime += (delta * simulationSpeedModifier);
		if (mergeSortElapsedTime >= mergeSortDelay)
		{
			mergeSortElapsedTime = 0;

			sortCurElem();

			if (curMergeElem->values.size() == MAX_VALUE_SIZE)
			{
				mergeSortState = MERGE_SORT_STATE::WAIT_ANIM;
				mergeSortElapsedTime = 0;
			}
			else
			{
				mergeSortState = MERGE_SORT_STATE::SEARCH;
			}
			curMergeElem = nullptr;
		}
	}
	else if (mergeSortState == MERGE_SORT_STATE::WAIT_ANIM)
	{
		mergeSortElapsedTime += (delta * simulationSpeedModifier);
		if (mergeSortElapsedTime >= mergeSortDelay)
		{
			mergeSortState = MERGE_SORT_STATE::CHECK;
			checkIndex = 0;
			this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Checking");
		}
	}
	else if (mergeSortState == MERGE_SORT_STATE::CHECK)
	{
		checkSort(delta);
	}
}

void SortScene::buildMergeElemTree(MergeElem * me, const int start, const int end, const int size)
{
	if (start == end)
	{
		//cocos2d::log("s = %d, e = %d, size = %d", start, end, size);
		me->values.clear();
		me->values.push_back(values.at(start));
		//cocos2d::log("leaf");
		me->leaf = true;
	}
	else
	{
		int mid = size / 2;
		//cocos2d::log("s = %d, e = %d, mid = %d, size = %d", start, end, mid, size);
		me->left = new MergeElem();
		me->left->parent = me;
		me->left->startIndex = start;
		me->left->endIndex = start + mid - 1;
		buildMergeElemTree(me->left, start, start + mid - 1, mid);

		me->right = new MergeElem();
		me->right->parent = me;
		me->right->startIndex = start + mid;
		me->right->endIndex = end;
		buildMergeElemTree(me->right, start + mid, end, end - (start + mid) + 1);
	}
}

MergeElem * SortScene::searchUnsortedElem(MergeElem* elem)
{
	if (elem == nullptr) return nullptr;

	if (elem->values.size() == 0)
	{
		// this element hasn't been sorted yet
		// Check if this element has children(left right) and check the size of it
		if (elem->left && elem->right)
		{
			if (elem->left->leaf && elem->right->leaf)
			{
				return elem;
			}
		}
	}

	auto left = searchUnsortedElem(elem->left);
	if (left)
	{
		return left;
	}
	else
	{
		auto right = searchUnsortedElem(elem->right);
		if (right)
		{
			return right;
		}
		else
		{
			return nullptr;
		}
	}
}

void SortScene::sortCurElem()
{
	if (curMergeElem)
	{
		curMergeElem->values.clear();

		int leftIndex = 0;
		int rightIndex = 0;
		int leftSize = curMergeElem->left->values.size();
		int rightSize = curMergeElem->right->values.size();

		while (leftIndex < leftSize || rightIndex < rightSize)
		{
			if (leftIndex < leftSize && rightIndex < rightSize)
			{
				if (curMergeElem->left->values.at(leftIndex) <= curMergeElem->right->values.at(rightIndex))
				{
					curMergeElem->values.push_back(curMergeElem->left->values.at(leftIndex));

					leftIndex++;
				}
				else
				{
					curMergeElem->values.push_back(curMergeElem->right->values.at(rightIndex));

					rightIndex++;
				}
			}
			else if (leftIndex < leftSize)
			{
				for (int i = leftIndex; i < leftSize; i++)
				{
					curMergeElem->values.push_back(curMergeElem->left->values.at(i));
				}
				break;
			}
			else if (rightIndex < rightSize)
			{
				for (int i = rightIndex; i < rightSize; i++)
				{
					curMergeElem->values.push_back(curMergeElem->right->values.at(i));
				}
				break;
			}
		}

		curMergeElem->leaf = true;
		delete curMergeElem->left;
		curMergeElem->left = nullptr;
		delete curMergeElem->right;
		curMergeElem->right = nullptr;

		int eIndex = 0;
		for (int i = curMergeElem->startIndex; i <= curMergeElem->endIndex; i++)
		{
			values.at(i) = curMergeElem->values.at(eIndex);

			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
			bars.at(i)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(values.at(i)) * 650.0f / barScaleYMul)));
			bars.at(i)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::YELLOW));

			eIndex++;
		}
	}
}

void SortScene::stepMergeSort()
{
	curMergeElem = searchUnsortedElem(mergeSortRoot);

	if (curMergeElem)
	{
		float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;
		for (int i = curMergeElem->left->startIndex; i <= curMergeElem->left->endIndex; i++)
		{
			bars.at(i)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::BLUE));
		}
		for (int i = curMergeElem->right->startIndex; i <= curMergeElem->right->endIndex; i++)
		{
			bars.at(i)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::BLUE));
		}
	}

	mergeSortState = MERGE_SORT_STATE::SORT;

	mergeSortElapsedTime = 0;
}

void SortScene::initBubbleSort()
{
	randomizeValues();
	resetBar();
	sortMode = SORT_MODE::BUBBLE;

	bubbleSortState = BUBBLE_SORT_STATE::NONE;

	curBubbleIndex = -1;

	animSpeed = 0.06f;

	bubbleSortDelay = 0.02f;
	bubbleSortElapsedTime = 0;

	checkIndex = 0;

	swapped = false;

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Sorting");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: Bubble Sort");
}

void SortScene::updateBubbleSort(const float delta)
{
	if (bubbleSortState == BUBBLE_SORT_STATE::NONE)
	{
		bubbleSortState = BUBBLE_SORT_STATE::NEXT;
	}
	else if (bubbleSortState == BUBBLE_SORT_STATE::NEXT)
	{
		if (paused) return;

		bubbleSortElapsedTime += (delta * simulationSpeedModifier);
		while (bubbleSortElapsedTime >= bubbleSortDelay)
		{
			bubbleSortElapsedTime -= bubbleSortDelay;

			stepBubbleSort();
		}
	}
	else if(bubbleSortState == BUBBLE_SORT_STATE::SWAP)
	{ 
		bubbleSortElapsedTime += (delta * simulationSpeedModifier);
		while (bubbleSortElapsedTime >= bubbleSortDelay)
		{
			bubbleSortElapsedTime -= bubbleSortDelay;

			int leftValue = values.at(curBubbleIndex);
			int rightValue = values.at(curBubbleIndex + 1);

			if (leftValue > rightValue)
			{
				values.at(curBubbleIndex) = rightValue;
				values.at(curBubbleIndex + 1) = leftValue;

				float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

				bars.at(curBubbleIndex)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(rightValue) * 650.0f / barScaleYMul), 1.0f));
				bars.at(curBubbleIndex + 1)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(leftValue) * 650.0f / barScaleYMul), 1.0f));

				swapped = true;
			}

			bubbleSortState = BUBBLE_SORT_STATE::NEXT;
			bubbleSortElapsedTime = 0;
		}
	}
	else if (bubbleSortState == BUBBLE_SORT_STATE::CHECK)
	{
		checkSort(delta);
	}
}

void SortScene::stepBubbleSort()
{
	curBubbleIndex++;

	float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

	if (curBubbleIndex == MAX_VALUE_SIZE - 1)
	{
		bars.at(curBubbleIndex)->setColor(cocos2d::Color3B::WHITE);
		bars.at(curBubbleIndex - 1)->setColor(cocos2d::Color3B::WHITE);
		curBubbleIndex = -1;

		if (swapped == false)
		{
			bubbleSortState = BUBBLE_SORT_STATE::CHECK;
			this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Checking");
			return;
		}
		else
		{
			swapped = false;
		}
	}
	else
	{
		if (curBubbleIndex > 0)
		{
			bars.at(curBubbleIndex - 1)->setColor(cocos2d::Color3B::WHITE);
		}

		bars.at(curBubbleIndex)->setColor(cocos2d::Color3B::BLUE);
		bars.at(curBubbleIndex + 1)->setColor(cocos2d::Color3B::BLUE);

		bubbleSortState = BUBBLE_SORT_STATE::SWAP;

		bubbleSortElapsedTime = 0;
	}
}

void SortScene::initQuickSort()
{
	randomizeValues();
	resetBar();
	sortMode = SORT_MODE::QUICK;

	quickSortState = QUICK_SORT_STATE::NONE;

	if (quickSortRoot) delete quickSortRoot;

	quickSortRoot = new QuickElem();
	quickSortRoot->values = values;

	animSpeed = 0.06f;

	checkIndex = 0;

	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Sorting");
	this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::CURRENT_SORT), "Current sort: Quick Sort");
}

void SortScene::updateQuickSort(const float delta)
{
	if (quickSortState == QUICK_SORT_STATE::NONE)
	{
		quickSortState = QUICK_SORT_STATE::PICK_PIVOT;
		curQuickElem = quickSortRoot;
		curQuickElem->startIndex = 0;
		curQuickElem->endIndex = values.size() - 1;
	}
	else if (quickSortState == QUICK_SORT_STATE::PICK_PIVOT)
	{
		float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

		if (curQuickElem->parent)
		{
			if (bars.at(curQuickElem->parent->pivotIndex)->getNumberOfRunningActions())
			{
				return;
			}


			/*
			if (curQuickElem->parent->parent == nullptr)
			{
				bars.at(values.size() - 1)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::WHITE));
			}
			else
			{
				bars.at(curQuickElem->parent->pivotIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::WHITE));
			}
			*/
			bars.at(curQuickElem->parent->pivotIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::WHITE));
		}

		int pivotIndex = curQuickElem->values.size() - 1;
		bars.at(curQuickElem->endIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::RED));

		curQuickElem->pivot = curQuickElem->values.back();
		curQuickElem->pivotIndex = pivotIndex;

		quickSortState = QUICK_SORT_STATE::PARTITION;
	}
	else if (quickSortState == QUICK_SORT_STATE::PARTITION)
	{
		if (paused) return;

		stepQuickSort();
	}
	else if (quickSortState == QUICK_SORT_STATE::MERGE)
	{
		if (bars.at(curQuickElem->pivotIndex)->getNumberOfRunningActions())
		{
			return;
		}

		assert(curQuickElem->values.size() >= 2);

		curQuickElem->values.clear();

		for (auto v : curQuickElem->left->values)
		{
			curQuickElem->values.push_back(v);
		}

		curQuickElem->values.push_back(curQuickElem->pivot);

		for (auto v : curQuickElem->right->values)
		{
			curQuickElem->values.push_back(v);
		}

		curQuickElem->sorted = true;
		curQuickElem->leaf = true;

		float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

		for (int i = curQuickElem->startIndex; i <= curQuickElem->endIndex; i++)
		{
			bars.at(i)->stopAllActions();
			bars.at(i)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::YELLOW));
		}

		curQuickElem = curQuickElem->parent;

		if (curQuickElem == nullptr)
		{
			curQuickElem = quickSortRoot;
			quickSortState = QUICK_SORT_STATE::CHECK;
		}
		else
		{
			if (curQuickElem->left->sorted && curQuickElem->right->sorted)
			{

			}
			else
			{
				quickSortState = QUICK_SORT_STATE::PARTITION;
			}
		}
	}
	else if (quickSortState == QUICK_SORT_STATE::CHECK)
	{
		checkSort(delta);
	}
}

void SortScene::stepQuickSort()
{
	if (curQuickElem->sorted == false)//values.size() >= 2)// && curQuickElem->left == nullptr && curQuickElem->right == nullptr)
	{
		if (curQuickElem->pivot == -1)
		{
			quickSortState = QUICK_SORT_STATE::PICK_PIVOT;
			return;
		}

		if (bars.at(curQuickElem->pivotIndex)->getNumberOfRunningActions())
		{
			return;
		}

		if (curQuickElem->left == nullptr && curQuickElem->right == nullptr)
		{
			curQuickElem->left = new QuickElem();
			curQuickElem->left->parent = curQuickElem;
			curQuickElem->right = new QuickElem();
			curQuickElem->right->parent = curQuickElem;

			int pivotVal = curQuickElem->values.back();

			curQuickElem->pivot = pivotVal;

			int index = 0;

			for (auto v : curQuickElem->values)
			{
				if (v != pivotVal)
				{
					if (v < pivotVal)
					{
						curQuickElem->left->values.push_back(v);
					}
					else
					{
						curQuickElem->right->values.push_back(v);
					}
				}

				index++;
			}

			int leftSize = curQuickElem->left->values.size();
			int rightSize = curQuickElem->right->values.size();

			curQuickElem->left->startIndex = curQuickElem->startIndex;
			curQuickElem->left->endIndex = curQuickElem->startIndex + (leftSize - 1);
			curQuickElem->pivotIndex = curQuickElem->startIndex + leftSize;
			curQuickElem->right->startIndex = curQuickElem->pivotIndex + 1;
			curQuickElem->right->endIndex = curQuickElem->endIndex;

			std::vector<int> tempValues;
			tempValues.insert(tempValues.end(), curQuickElem->left->values.begin(), curQuickElem->left->values.end());
			tempValues.push_back(curQuickElem->pivot);
			tempValues.insert(tempValues.end(), curQuickElem->right->values.begin(), curQuickElem->right->values.end());

			float ssmFlip = (2.0f - simulationSpeedModifier) * 2.0f;

			int barIndex = curQuickElem->left->startIndex;
			for (unsigned int i = 0; i < tempValues.size(); i++)
			{
				bars.at(barIndex)->runAction(cocos2d::TintTo::create(animSpeed * ssmFlip, cocos2d::Color3B::WHITE));

				bars.at(barIndex)->runAction(cocos2d::ScaleTo::create(animSpeed * ssmFlip, barScaleX, (static_cast<float>(tempValues.at(i)) * 650.0f / barScaleYMul), 1.0f));
				barIndex++;
			}

			bool leftLeaf = false;
			if (curQuickElem->left->values.size() <= 1)
			{
				curQuickElem->left->leaf = true;
				curQuickElem->left->sorted = true;
				leftLeaf = true;
			}

			bool rightLeaf = false;
			if (curQuickElem->right->values.size() <= 1)
			{
				curQuickElem->right->leaf = true;
				curQuickElem->right->sorted = true;
				rightLeaf = true;
			}

			if (leftLeaf && rightLeaf)
			{
				quickSortState = QUICK_SORT_STATE::MERGE;
			}
			else if (leftLeaf)
			{
				curQuickElem = curQuickElem->right;
			}
			else if (rightLeaf)
			{
				curQuickElem = curQuickElem->left;
			}
			else
			{
				curQuickElem = curQuickElem->left;
			}
		}
		else
		{
			bool l = curQuickElem->left->leaf;
			bool r = curQuickElem->right->leaf;

			if (l && r)
			{
				quickSortState = QUICK_SORT_STATE::MERGE;
			}
			else if (l)
			{
				curQuickElem = curQuickElem->right;
			}
			else if (r)
			{
				curQuickElem = curQuickElem->left;
			}
			else
			{
				curQuickElem = curQuickElem->left;
			}
		}
	}
	else
	{
		if (curQuickElem->parent->right)
		{
			curQuickElem = curQuickElem->parent->right;
		}
		else
		{
			curQuickElem = curQuickElem->parent;
		}
	}
}

void SortScene::checkSort(const float delta)
{
	checkElapsedTime += (delta * simulationSpeedModifier);

	if (checkElapsedTime <= checkSpeed)
	{
		checkElapsedTime -= checkSpeed;

		bars.at(checkIndex)->runAction(cocos2d::Sequence::createWithTwoActions(cocos2d::TintTo::create(checkSpeed, cocos2d::Color3B::GREEN), cocos2d::TintTo::create(checkSpeed, cocos2d::Color3B::YELLOW)));

		checkIndex++;

		if (checkIndex == MAX_VALUE_SIZE)
		{
			switch (sortMode)
			{
			case SortScene::SORT_MODE::SELECTION:
				selectionSortState = SELECTION_SORT_STATE::FINISHED;
				break;
			case SortScene::SORT_MODE::INSERTION:
				insertionSortState = INSERTION_SORT_STATE::FINISHED;
				break;
			case SortScene::SORT_MODE::MERGE:
				mergeSortState = MERGE_SORT_STATE::FINISHED;
				break;
			case SortScene::SORT_MODE::BUBBLE:
				bubbleSortState = BUBBLE_SORT_STATE::FINISHED;
				break;
			case SortScene::SORT_MODE::QUICK:
				quickSortState = QUICK_SORT_STATE::FINISHED;
				break;
			case SortScene::SORT_MODE::NONE:
			default:
				break;
			}

			this->labelsNode->updateLabel(static_cast<int>(CUSTOM_LABEL_INDEX::STATUS), "Status: Finished");
		}
	}
}

void SortScene::checkMouseOver(const cocos2d::Vec2 & mousePos)
{
	for (unsigned int i = 0; i < bars.size(); i++)
	{
		if (bars.at(i)->getBoundingBox().containsPoint(mousePos))
		{
			if (hoveringBarIndex == i)
			{
				return;
			}
			else
			{
				hoveringBarIndex = i;

				std::string str = "Index: " + std::to_string(i) + ", value = " + std::to_string(values.at(i));

				switch (sortMode)
				{
				case SortScene::SORT_MODE::SELECTION:
				{
					if (i <= sortedIndex && sortedIndex >= 0)
					{
						str += "State: Sorted";
					}
					else
					{
						str += "State: Unsorted";
					}
				}
				break;
				case SortScene::SORT_MODE::INSERTION:
				{
					if (i <= curInsertionSortIndex)
					{
						str += "State: Sorted";
					}
					else
					{
						str += "State: Unsorted";
					}
				}
				break;
				case SortScene::SORT_MODE::QUICK:
					break;
				case SortScene::SORT_MODE::MERGE:
				case SortScene::SORT_MODE::BUBBLE:
				case SortScene::SORT_MODE::NONE:
				default:
					return;
					break;
				}

				barInfoLabel->setString(str);

				return;
			}
		}
	}

	barInfoLabel->setString("");
	hoveringBarIndex = -1;
}

void SortScene::replaceBars()
{
	for (int i = 0; i < 100; i++)
	{
		bars.at(i)->setScaleX(barScaleX);
		if (i < MAX_VALUE_SIZE)
		{
			bars.at(i)->setScaleY(static_cast<float>(values.at(i)) * 650.0f / barScaleYMul);
		}
		else
		{
			bars.at(i)->setScaleY(0);
		}
		bars.at(i)->setPosition(cocos2d::Vec2(35.0f + (static_cast<float>(i) * barScaleX), 36.0f));
		if (i < MAX_VALUE_SIZE)
		{
			bars.at(i)->setVisible(true);
		}
		else
		{
			bars.at(i)->setVisible(false);
		}
	}
}

void SortScene::onArraySizeSliderClick(cocos2d::Ref * sender)
{
	// Click ended. Get value
	MAX_VALUE_SIZE = this->barCountSliderNode->sliderLabels.back().slider->getPercent() + 10;

	const float MAX_VALUE_SIZE_F = static_cast<float>(MAX_VALUE_SIZE);
	barScaleX = 650.0f / MAX_VALUE_SIZE_F;
	barScaleYMul = MAX_VALUE_SIZE_F;

	reset();
	replaceBars();
}

void SortScene::onSliderClick(cocos2d::Ref* sender)
{
	// Click ended. Get value
	float percentage = static_cast<float>(this->sliderLabelNode->sliderLabels.back().slider->getPercent());
	//50% = 1.0(default. So multiply by 2.
	percentage *= 2.0f;
	// 0% == 0, 100% = 1.0f, 200% = 2.0f
	if (percentage == 0)
	{
		// 0 will make simulation stop
		percentage = 1;
	}
	//. Devide by 100%
	percentage *= 0.01f;

	// apply
	this->simulationSpeedModifier = percentage;
}

void SortScene::onExit()
{
	cocos2d::Scene::onExit();
	// Uncomment this if you are using initInputListeners()
	releaseInputListeners(); 

	if (mergeSortRoot)
		delete mergeSortRoot;

	if (quickSortRoot)
	{
		delete quickSortRoot;
	}
}
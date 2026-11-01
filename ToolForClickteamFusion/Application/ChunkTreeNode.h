#pragma once

#include "../Chunk/Chunk.h"

using namespace CTT::Chunk;

// Chunk를 커스텀하게 표현하기 위한 노드 클래스
class ChunkTreeNode
{
private:
	string idStr;
	ChunkInfo value;					// chunk의 정보
	UINT32 offset;						// 아카이브 상의 오프셋
	ChunkTreeNode* parent;
	vector<ChunkTreeNode*> children;
	bool isSelected;

public:
	// 생성자
	ChunkTreeNode()
	{
		// end chunk로 초기화 (더미용)
		idStr = "";
		value.type = ChunkType::CHUNK_END;
		value.flag = 0;
		value.size = 0;
		offset = 0;
		// parent 초기화
		parent = NULL;
		// children 초기화
		children.clear();
		// 선택여부 초기화
		isSelected = false;
	}

	// 소멸자
	~ChunkTreeNode()
	{
		// 부모 노드의 포인터에도 null 로 박아버려야 할것 같다
		if (parent != NULL) {
			vector<ChunkTreeNode*> parentChildrenPointer = parent->getChildren();
			for (UINT32 i = 0; i < parentChildrenPointer.size(); i++) {
				if (parentChildrenPointer[i] == this) {
					parentChildrenPointer[i] = NULL;
					break;
				}
			}
		}
		parent = NULL;
		for (UINT32 i = 0; i < children.size(); i++) {
			if (children[i] != NULL) {
				delete children[i];
				children[i] = NULL;
			}
		}
		children.clear();
	}

	string& getIdStr()
	{
		return idStr;
	}
	void setIdStr(string idStr)
	{
		this->idStr = idStr;
	}

	ChunkInfo& getValue()
	{
		return value;
	}
	void setValue(ChunkInfo& value)
	{
		memcpy(&(this->value), &value, sizeof(ChunkInfo));
	}

	UINT32 getOffset()
	{
		return offset;
	}
	void setOffset(UINT32 offset)
	{
		this->offset = offset;
	}

	ChunkTreeNode* getParent()
	{
		return parent;
	}
	void setParent(ChunkTreeNode* parent)
	{
		this->parent = parent;
	}

	vector<ChunkTreeNode*>& getChildren()
	{
		return children;
	}

	void appendChild(ChunkTreeNode* child)
	{
		children.push_back(child);
		child->setParent(this);
	}

	bool selected()
	{
		return isSelected;
	}
	void setSelected(bool isSelected)
	{
		this->isSelected = isSelected;
	}

	// 모든 노드의 선택상태를 일괄 변경
	void setAllNodeSelected(bool isSelected)
	{
		this->setSelected(isSelected);
		for (UINT32 i = 0; i < children.size(); i++) {
			children[i]->setAllNodeSelected(isSelected);
		}
	}
};


// ChunkTreeNode를 관리하기 위한 클래스
class ChunkTreeNodeContainer
{
private:
	// 루트 노드
	ChunkTreeNode* root;
	// 현재 가리키는 노드 (노드 트리 구성시에만 쓰임)
	ChunkTreeNode* currentNodePointer;
	// 현재 선택된 노드
	ChunkTreeNode* selectedNode;

public:
	// 생성자
	ChunkTreeNodeContainer()
	{
		root = currentNodePointer = selectedNode = NULL;
	}

	// 소멸자
	~ChunkTreeNodeContainer()
	{
		release();
	}

	// 데이터 초기화 함수
	void init()
	{
		root = new ChunkTreeNode();
		currentNodePointer = root;
	}

	// 데이터 해제 함수
	void release()
	{
		if (root != NULL) {
			delete root;
		}
		root = currentNodePointer = selectedNode = NULL;
	}

	// 데이터 클리어 함수
	void clear()
	{
		release();
		init();
	}

	// depth 깊이를 추가하는 함수
	void plusDepth()
	{
		vector<ChunkTreeNode*> currentNodeChildren = currentNodePointer->getChildren();
		// 자식노드 리스트가 있으면 그중 맨 마지막 노드를 현재 가리키는 노드로 지정
		if (currentNodeChildren.size() != 0) {
			currentNodePointer = currentNodeChildren[currentNodeChildren.size() - 1];
		}
	}

	// depth 깊이를 덜어내는 함수
	void minusDepth()
	{
		// 부모가 있다면 부모 노드를 현재 가리키는 노드로 지정, 없으면 그대로
		if (currentNodePointer->getParent() != NULL) {
			currentNodePointer = currentNodePointer->getParent();
		}
	}

	// 루트 노드 포인터를 반환하는 함수
	ChunkTreeNode* getRootNodePointer()
	{
		return root;
	}

	// 현재 가리키는 노드에 child 추가
	void appendChildToCurrentNode(ChunkTreeNode* child)
	{
		currentNodePointer->appendChild(child);
	}

	void setSelectedNode(ChunkTreeNode* node)
	{
		this->selectedNode = node;
	}
	ChunkTreeNode* getSelectedNode()
	{
		return selectedNode;
	}

};
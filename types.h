#pragma once

struct StringTable {
	char* name;
	int columnCount;
	int rowCount;
};

struct LootItem {
	int m_itemId;
	int m_itemQuantity;
};

class client_t {
private:
	virtual void unk() = 0;
public:
	virtual void SendServerCommand(int type, const char* command) = 0;
};

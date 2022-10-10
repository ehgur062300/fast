#define MEGABYTE_SIZE 1024*1024
#define SECTOR_SIZE 512
#define BLOCK_SIZE 4
#define LOG_RATIO 0.4

#include <iostream>
#include <queue>

using namespace std;

// 예외처리 : 입력 데이터 타입
bool input_exeption(int a) {
	if (!cin) {
		cout << "error : input type is difficult \n";
		cin.clear();
		cin.ignore(10,'\n');
	}
};

// Sector 구조
struct Sector {
	char data[SECTOR_SIZE] = {};
	bool is_using = false;
	bool rw_using = false;
};

// Block 구조
struct Block {
	Sector sector[BLOCK_SIZE] = {};
	int wear_level = 0;
};

struct BW_pair {
	int wear_level = 0;
	int block_index = 0;
	bool operator<(const BW_pair& a)const {
		if (this->wear_level == a.wear_level) { return this->block_index > a.block_index; }
		return this->wear_level > a.wear_level;
	}
};

// 블록 맵핑
typedef struct BLOCK_MAPPING_STRUCTURE {
	int pbn = -1;
}BMP;

// 순차적 쓰기 맵핑
typedef struct SEQUENTIAL_WRITE_MAPPING_STRUCTURE {
	int sector[BLOCK_SIZE] = {};
	int pbn = -1;
	int lsn = -1;
}SWMP;

// 무작위 쓰기 맵핑
typedef struct RANDOM_WRITE_MAPPING_STRUCTURE {
	int pbn = -1;
	int lsn = -1;
}RWMP;

// 플레쉬메모리 구조
class Flash_Memory {
public :
	bool init();
	bool flash_write(const int block_idx, const int sector_idx, const char data[SECTOR_SIZE]);
	bool flash_read(const int block_idx, const int sector_idx);
	bool flash_erase(const int block_idx);
	int get_memory_size() { return flash_memory_size; }

	Flash_Memory() {};
	~Flash_Memory() {
		cout << "delete flash_memory \n\n";
		delete[] flash_memory;
	};

private :
	Block* flash_memory = nullptr;
	int flash_memory_size = 0;
};

class FTL {
public : 
	bool init();
	bool ftl_write(const int index, const char data[BLOCK_SIZE]);
	bool ftl_read(const int index);

	bool init_block_Q(const int flash_memory_size);
	

private:
	/*
	bool merge_operation(const int lbn, const int pbn, const int log_pbn);
	bool switch_operation(const int lbn, const int pbn, const int log_pbn);
	*/
	Flash_Memory flash_memory;
	priority_queue<BW_pair> free_block_Q;
	BMP* block_mapping_table = nullptr;
	SWMP sequential_mapping_table;
	RWMP* random_mapping_table = nullptr;
	int log_begin = 0;
};

bool FTL::init() {
	int flash_memory_size = flash_memory.get_memory_size();
	int block_mapping_table_size = flash_memory_size;
	int random_mapping_table_size = (int)(LOG_RATIO * flash_memory_size)-1; // -1은 sw_mapping_table_size를 제외한 것

	block_mapping_table = new BMP[block_mapping_table_size];
	if (block_mapping_table == nullptr) {
		cout << "FTL::init :: fail to create block_mapping_table\n";
		return false;
	}

	random_mapping_table = new RWMP[random_mapping_table_size];
	if (random_mapping_table == nullptr) {
		cout << "FTL::init :: fail to create random_mapping_table\n";
		return false;
	}
	
	cout << "FTL::init :: init free_block_Q, size : " << free_block_Q.size() << "\n";
	return true;
};

bool FTL::init_block_Q(const int flash_memory_size) {
	log_begin = (int)((1 - LOG_RATIO) * flash_memory_size); // log block의 시작점
	for (int i = 0; i < flash_memory_size; i++) {
		if (i < data_block_ratio) { data_block_Q.push({ 0, i }); }
		else { log_block_Q.push({ 0, i }); }
	}
	if (log_block_Q.size() == 0 || data_block_Q.size() == 0) { return false; }
	return true;
};
// 플레쉬메모리 생성
bool Flash_Memory::init() {
	int MB = 0;
	cout << "Flash_Memory::init:flash_memory_size(MB) : "; cin >> MB;
	flash_memory_size = MB * (MEGABYTE_SIZE / sizeof(Block));
	flash_memory = new Block[flash_memory_size];
};

// 플레쉬메모리 쓰기
bool Flash_Memory::flash_write(const int block_index, const int sector_index, const char data[SECTOR_SIZE]) {
	// 예외처리 : 입력한 블록 or 섹터 사이즈가 범위를 벗어남
	if (block_index > flash_memory_size || block_index < 0 || sector_index > BLOCK_SIZE || sector_index < 0) {
		cout << "FLASH_MEMORY::flash_write :: try to access out of range block_index : <<" << block_index << ", sector_index : " << sector_index << "\n";
		return false;
	}
	// 예외처리 : 쓰기 연산이 일어나는 해당 블록의 섹터에 이미 데이터가 있음
	if (flash_memory[block_index].sector[sector_index].is_using == true
		|| flash_memory[block_index].sector[sector_index].data[0] != '\0') {
		cout << "FLASH_MEMORY::flash_write( " << block_index << ", " << sector_index << " ) :: already using now\n";
		return false;
	}
	// 쓰기
	strcpy_s(flash_memory[block_index].sector[sector_index].data, data);
	cout << "FLASH_MEMORY::flash_write( " << block_index << ", " << sector_index << " ) :: " << flash_memory[block_index].sector[sector_index].data << "\n";
	flash_memory[block_index].sector[sector_index].is_using = true;
	return true;
};

bool Flash_Memory::flash_read(const int block_index, const int sector_index) {
	// 예이 처리 : 입력 인덱스가 블록 or 섹터 범위를 넘어갈때
	if (block_index > flash_memory_size || block_index < 0 || sector_index > BLOCK_SIZE || sector_index < 0) {
		cout << "FLASH_MEMORY::flash_erase :: try to access out of range block_index : <<" << block_index << ", sector_index : " << sector_index << "\n";
		return false;
	}
	// 해당 섹터가 유효한 정보이면 읽기
	if (flash_memory[block_index].sector[sector_index].is_using == true) {
		cout << "FLASH_MEMORY::flash_read( " << block_index << ", " << sector_index << " ) :: " << flash_memory[block_index].sector[sector_index].data << "\n";
		return true;
	}
	// 해당 섹터에 유효하지 않은 데이터가 있을때
	if (flash_memory[block_index].sector[sector_index].data[0] != '\0') {
		cout << "FLASH_MEMORY::flash_read( " << block_index << ", " << sector_index << " ) :: data was updated\n";
		return false;
	}
	// 해당 섹터에 데이터가 존재하지 않을때
	cout << "FLASH_MEMORY::flash_read( " << block_index << ", " << sector_index << " ) :: no data\n";
	return true;
};

bool Flash_Memory::flash_erase(const int block_index) {
	// 예외처리 : 입력된 인덱스가 블록 사이즈를 넘어갈때
	if (block_index < 0 || block_index > flash_memory_size) {
		cout << "FLASH_MEMORY::flash_erase :: try to access out of range block_index : <<" << block_index << "\n";
		return false;
	}
	// 해당 블록의 섹터 초기화
	for (int i = 0; i < BLOCK_SIZE; i++) {
		strcpy_s(flash_memory[block_index].sector[i].data, "");
		flash_memory[block_index].sector[i].is_using = false;
		flash_memory[block_index].sector[i].rw_using = false;
	}
	// 블록 마모도 증가
	flash_memory[block_index].wear_level++;
	cout << "FLASH_MEMORY::flash_erase( " << block_index << " ) :: complete\n";
	return true;
};










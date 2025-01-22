#include <iostream>
#include <vector>
#include <array>


enum class CellType {
	TOGUZ_KORGOOL,
	Type2,
	Type3,
	Type4,

};


class Board {

	public:
		Board(size_t size, int ball_count) {
		//	player1.reserve(size);
		//	player2.reserve(size);

		//	for ()
		
		}
		//std::vector<int> get_board() {
		
		//}
		std::vector<int>& get_player1_board() {
			return player1;
		}

		std::vector<int>& get_player2_board() {
			return player2;
		}

		void print_boards() {
			for (const auto& cell_count : player1) {
				std::cout << cell_count << " ";
			}
			std::cout << std::endl;
			for (const auto& cell_count : player2) {
				std::cout << cell_count << " ";
			}
			std::cout << std::endl;
		}

	private:
		std::vector<int> player1 {9, 9, 9, 9, 9, 9, 9, 9, 9};
		std::vector<int> player2 {9, 9, 9, 9, 9, 9, 9, 9, 9};	

};


int main() {

	Board board(9, 9);

	board.print_boards();

	return 0;
}

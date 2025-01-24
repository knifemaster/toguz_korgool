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

		
		}

		Board(Board& other) {
		
			player1 = other.player1;
			player2 = other.player2;

			std::cout << "calling copy constructor" << std::endl;
		}

		Board& operator=(Board& other) {
			player1 = other.player1;
			player2 = other.player2;

			//std::swap(player1, other.player1);
			//std::swap(player2, other.player2);
			std::cout << "copy assignment" << std::endl;

			return *this;
		}


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

		void add_to_cells(int position, int count_balls) {
			

		}

		void move(int position, bool color) {
			// color black(false) or white(true)
		
			if (color) {
				std::cout << player1[cell_count - position] << std::endl;
				int count = player1[cell_count - position];
				player1[cell_count - position] = 0;
				int k = count - (cell_count - position);
				std::cout << "k = " << k << std::endl;
				for (int i = cell_count - position; i >= 0; --i) {
					player1[i]++;

				}

				for (int i = 0; i < k; i++) {
					player2[i]++;	
				}

			}
			else {
				
			}

		}

	private:
		//std::vector<unsigned short> or byte
		int cell_count = 9;
		std::vector<int> player1 {9, 9, 9, 9, 9, 20, 9, 9, 9};
		std::vector<int> player2 {9, 9, 9, 9, 9, 9, 9, 9, 9};	

};


int main() {

	//Board board2(9, 9);
	Board board(9, 9);
	//Board board2(board);
	//board2 = board;

	board.move(4, true);
	board.print_boards();
	//board.print_boards();

	return 0;
}

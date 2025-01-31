#include <iostream>
#include <vector>
#include <array>
#include <utility>
#include <optional>

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

		int add_to_cells(int position, int count_balls, bool is_white) {
			if (is_white) {
				for (int i = position; i >= 0; i--) {
					player1[i]++;				
				}
			} else {
				for (int i = position; i <= player2.size(); i++) {
					player2[i]++;
				}
			
			}


			for (int i = position; i < )	

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
		//std::vector<bool> ace1 {false, false, false, false, false, false, false, false, false};
		//std::vector<bool> ace2 {false, false, false, false, false, false, false, false, false};
		
		std::vector<std::optional<bool>> ace1 {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
		//std::vector<bool> ace_position {false, false, false, false, false, false, false, false, false};
		//std::vector<bool> ace_position2 {false, false, false, false, false, false, false, false, false};
		
		std::vector<std::optional<bool>> ace;	

};


int main() {

	//Board board2(9, 9);
	Board board(9, 9);

	//board2 = board;

	board.move(4, true);
	board.print_boards();
	//board.print_boards();

	return 0;
}

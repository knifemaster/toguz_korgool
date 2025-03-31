#include <iostream>
#include <vector>
#include <array>
#include <utility>
#include <optional>
#include <memory>


class Board {

	public:
		Board(int id) : game_id(id) {
            
		
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

		int add_to_cells(int position, int& count_balls, bool is_white) {
			int k;
			int counter = 0;
			
			if (count_balls >= 9) {
				k = count_balls - 9;	
			}
			else {
				k = count_balls;
			}

			if (is_white) {
				for (int i = position; i >= 0; i--) {
					if (counter)
					counter++;
					player1[i]++;				
				}
			} else {
				for (int i = position; i <= player2.size(); i++) {
					player2[i]++;
				}
			
			}


			//for (int i = position; i < )	

		}

		void move(int position, bool color) {
			// color black(false) or white(true)
				
			if (color) {
				std::cout << player1[cell_count - position] << std::endl;
				int count = player1[cell_count - position];
				player1[cell_count - position] = 0;
				int k = count - (cell_count - position);
				std::cout << "k = " << k << std::endl;
				
				bool color_side = color;

				while (count > 0) {
					if (color_side) {
						std::cout << "white" << std::endl;
						color_side = !color_side;
					} else {
						std::cout << "black" << std::endl;
						color_side = !color_side;
					}
			
					std::cout << count << std::endl;
					count -= 9;
					std::cout << count << std::endl;
					std::cout << "%" << count % 9 << std::endl;
					//int add_to_cells(int position, int& count_balls, bool is_white) {

				
				}
				
				
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
		int cell_count = 9;
		std::vector<int> player1 {9, 9, 9, 9, 9, 20, 9, 9, 9};
		std::vector<int> player2 {9, 9, 9, 9, 9, 9, 9, 9, 9};	
		
		std::vector<std::optional<bool>> ace;	

};

class Game {
    public:
        Game(int game_id, int game_status) : game_id(game_id), game_status(game_status) {}

        int get_game_id() const {
            return game_id;
        }

        int get_game_status() const {
            return game_status;
        }

        void move(int position, bool color ) {
            board.move(postion, color);
        }

    private:
        int game_id;
        int game_status; // пока инт
        Board board;

};


class World {
    public:
        void World() {};

        void add_game_to(int game_id) {
            games.insert(game_id, Game{});
        }

        

    private: 
        std::unordered_map<int, Game> games;
}


int main() {

	//Board board2(9, 9);
	Board board(9, 9);

	//board2 = board;

	board.move(4, true);
	board.print_boards();
	//board.print_boards();

	return 0;
}

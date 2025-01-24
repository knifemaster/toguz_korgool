#include <iostream>
#include <memory>
#include <vector>
#include <array>
//#include <Board.h>

class Game {

	public:
		GameType game_type;

		void move(int move_position) {
			for (const auto& cell : board)	
				std::cout << cell << " ";
		}

	private:
		Board board(9, 9);
		int game_id;

}






int main() {


	return 0
}

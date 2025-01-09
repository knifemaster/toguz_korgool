#include <iostream>
#include <chrono>
#include <atomic>


namespace utility {
	class Clock {
	
		public:
			void init_clock() {
				minutes = 0;
				seconds = 0;
				running = false
			}

			void set_clock_time(int min, int sec) {
				
				if (min > 5 && sec > 59) {
					std::cout << "incorrect time"<< std::endl;
					minutes = 4;
					seconds = 59
				}
				else {
					minute = min;
					seconds = sec
				}
				
				
			}

			void start_clock() {
				running = true;

				while (running && (minutes > 0 || seconds > 0) {
					if (seconds == 0) {
						minutes--;
						seconds = 59;
					} else {
						seconds--;
					}		

					std::cout << "minutes remaining " << minutes << std::endl;
					std::cout << "seconds remaining " << seconds << std::endl;
					
					std::this_thread::sleep_for(std::chrono::seconds(1));

				}
					
			}

			void stop_clock() {
				running = false;
				std::cout << "clock stopped" << std::endl;
			
			}

		private:
			int minutes;
			int seconds;
			std::atomic<bool> running;

	}		
}

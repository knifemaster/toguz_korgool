#include <iostream>
#include <chrono>
#include <atomic>


namespace utility {
	class Clock {
	
		public:
			void init_clock() {
				m_minutes = 0;
				m_seconds = 0;
				m_running = false;
			}

			void set_clock_time(int min, int sec) {
				
				if (min > 5 && sec > 59) {
					std::cout << "incorrect time"<< std::endl;
					m_minutes = 4;
					m_seconds = 59;
				}
				else {
					m_minutes = min;
					m_seconds = sec;
				}
				
				
			}

			void start_clock() {
				m_running = true;

				while (m_running && (m_minutes > 0 || m_seconds > 0) {
					if (m_seconds == 0) {
						m_minutes--;
						m_seconds = 59;
					} else {
						m_seconds--;
					}		

					std::cout << "minutes remaining " << m_minutes << std::endl;
					std::cout << "seconds remaining " << m_seconds << std::endl;
					
					std::this_thread::sleep_for(std::chrono::seconds(1));

				}
					
			}

			void stop_clock() {
				m_running = false;
				std::cout << "clock stopped" << std::endl;
			
			}

		private:
			int m_minutes = 0;
			int m_seconds = 0;
			std::atomic<bool> m_running;

	};		
}

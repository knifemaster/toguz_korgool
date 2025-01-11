#include <boost/date_time/gregorian/gregorian.hpp>


namespace utility {
	class DateAssistant {
		public:
			DateAssistant (std::string& date_) : date(boost::gregorian::from_simple_string(date_)) { }

		
			boost::gregorian::date get_date() {
				return date;
		    	}

		
			std::string get_date_string() {
				std::string str_date = boost::gregorian::to_simple_string(date);
				return str_date;
		    	}
	
	
			std::string get_current_date() {
				boost::gregorian::date current_date = boost::gregorian::day_clock::local_day();
				std::string str_date = boost::gregorian::to_simple_string(current_date);
				return str_date;
		    	}
		    
		
			void add_months(size_t months) {
				date += boost::gregorian::months(months);
		    	}

			
		    	void add_days(size_t days) {
				date += boost::gregorian::days(days);
		    	}
		    
		
			boost::gregorian::date difference_dates(std::string& other_date) {
				boost::gregorian::date date_diff = boost::gregorian::from_simple_string(other_date)
					boost::gregorian::date date_diff -= date;
			        //date_diff = date - other_date;
				return date_diff;
		    	}
	    
		private:
			boost::gregorian::date date;
	};

}

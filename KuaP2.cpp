/* P2 - Looking for Group Synchronization
	Created by: Kua, Miguel Carlo F. S11
*/

// Headers
#include <iostream>
#include <iomanip>
#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <random>

// Summary 
struct InstanceStats {
	int count = 0;
	int totalTime = 0;
	bool isActive = false;
};


std::mutex mtx;
std::condition_variable cv;
std::vector<InstanceStats> instanceStats;
std::atomic<int> InstancesActive = 0;
int InstancesMax;
bool keepRunning = true;

std::string currentTimestamp() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	char buf[100];
	std::tm now_tm;
	localtime_s(&now_tm, &now_c); // safer version of localtime
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &now_tm);
	return std::string(buf);
}

void writeSummaryStats(const std::string& filename)
{
	std::ofstream summary(filename);
	summary << "--------------------------------------\n D  U  N  G  E  O  N      S  T  A  T  S \n--------------------------------------\n";
	// Write instances served and runtime
	for (int i = 0; i < InstancesMax; i++)
	{
		summary << "Instance" << i + 1 << " served " << instanceStats[i].count << " parties for a total of " << instanceStats[i].totalTime << " seconds.\n";
	}
	summary.close();
	std::cout << "\nDungeon Summary Stats written to " << filename << "\n";
}

// Read config contents
std::unordered_map<std::string, int> readConfig(const std::string& filename)
{
	std::unordered_map<std::string, int> config;
	std::ifstream file(filename);
	std::string line;

	while (std::getline(file, line))
	{
		std::stringstream ss(line);
		std::string key;
		int value;

		// Input validation
		if (std::getline(ss, key, '=') && ss >> value)
		{
			// Handle negative inputs
			if (value < 0) {
				std::cerr << "Error: Negative integer found in " << key << "is not allowed." << std::endl;
				exit(1);
			}
			config[key] = value;
		}
	}

	// Check if all keys are represented
	std::vector<std::string> required = { "n","t","h","d","t1","t2" };
	for (const auto& key : required)
	{
		if (config.find(key) == config.end())
		{
			std::cerr << "Error: Missing configuration parameter: " << key << std::endl;
			exit(1);
		}
	}

	// Check min max time
	if (config["t1"] > config["t2"])
	{
		std::cerr << "Error: Change Minimum time to be less than or equal than Maximum Time" << std::endl;
		exit(1);
	}

	return config;

}

// Run Instance
void runInstance(int id, int duration)
{
	{
		// Count runtime
		std::lock_guard<std::mutex> lock(mtx);
		instanceStats[id].isActive = true;
		instanceStats[id].count++;
		instanceStats[id].totalTime += duration;
		std::cout << "\n[" << currentTimestamp() << "] Instance " << id + 1 << ": active for " << duration << " seconds.\n";
	}
	std::this_thread::sleep_for(std::chrono::seconds(duration));
	{
		// Active to empty instance
		std::lock_guard<std::mutex> lock(mtx);
		instanceStats[id].isActive = false;
		InstancesActive--;
		std::cout << "\n[" << currentTimestamp() << "] Instance " << id + 1 << ": now empty.\n";
		std::cout << "[" << currentTimestamp() << "] [INSTANCE STATUS]\n";
			for (int i = 0; i < InstancesMax; ++i) {
				std::cout << "Instance " << i + 1 << ": " << (instanceStats[i].isActive ? "ACTIVE" : "EMPTY") << "\n";
			}
	}
	cv.notify_one();
}

// Create and Assign Party to Instance Function
void createParty(int& tanks, int& healers, int& dps, std::uniform_int_distribution<>& distr, std::mt19937& gen, std::vector<std::thread>& dThreads)
{
	static int nextInstanceIndex = 0;
	while (true)
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [&] { return InstancesActive < InstancesMax || !(tanks >= 1 && healers >= 1 && dps >= 3); });
		// Check if the current amount of players meet the party requirements
		if (tanks >= 1 && healers >= 1 && dps >= 3)
		{
			tanks--;
			healers--;
			dps -= 3;

			// Run through all instances
			for (int i = 0; i < InstancesMax; ++i)
			{
				// Put Party in Instance
				int index = (nextInstanceIndex + i) % InstancesMax;
				if (!instanceStats[index].isActive)
				{
					int duration = distr(gen);
					InstancesActive++;
					// Generate the instance and start instance runtime
					dThreads.emplace_back(runInstance, index, duration);
					nextInstanceIndex = (index + 1) % InstancesMax;
					break;
				}
			}
		}
		else
		{
			// 
			keepRunning = false;
			cv.notify_all();
			break;
		}
	}
}


// Check Instance
void checkInstance()
{
	while (keepRunning)
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::lock_guard<std::mutex>lock(mtx);
		std::cout << "\n[" << currentTimestamp() << "] [INSTANCE STATUS]\n";
		for (int i = 0; i < InstancesMax; i++)
		{
			std::cout << "Instance " << i + 1 << ": " << (instanceStats[i].isActive ? "ACTIVE" : "EMPTY") << "\n";
		}
	}
}


int main() {
	auto config = readConfig("config.txt");
	InstancesMax = config["n"];
	int t = config["t"]; // Get Tank
	int h = config["h"]; // Get Healer
	int d = config["d"]; // Get DPS
	int t1 = config["t1"]; // Get Minimum Time
	int t2 = config["t2"]; // Get Maximum Time

	//
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distr(t1, t2);

	instanceStats.resize(InstancesMax);
	std::vector<std::thread> dThreads;
	std::thread mThread(checkInstance);

	createParty(t, h, d, distr, gen, dThreads);
	for (auto& t : dThreads)
	{
		if (t.joinable()) t.join();
	}

	// Create threads for dungeons
	if (mThread.joinable()) mThread.join();

	writeSummaryStats("summary.txt");
	return 0;
}
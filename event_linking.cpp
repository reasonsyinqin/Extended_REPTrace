#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp> 
#include <chrono>

using json = nlohmann::json;
using namespace std;

struct Event {
    int event_id;
    int ftype;
    string uuid;
    string unit_uuid;
};

json loadJsonFile(const string &file_path) {
    std::ifstream file(file_path);
    json data;
    file >> data;
    return data;
}

void linkEvents(const string &uuid_file, const string &unit_uuid_file, int num_events) {

    json uuid_data = loadJsonFile(uuid_file);
    json unit_uuid_data = loadJsonFile(unit_uuid_file);

    vector<int> event_parent(num_events + 1, -1); 

    unordered_map<string, int> last_uuid_event;
    unordered_map<string, int> last_unit_uuid_event;
    // start time
    auto start = std::chrono::high_resolution_clock::now();
    // UUID events
    for (const auto &uuid_entry : uuid_data.items()) {
        const string &uuid = uuid_entry.key();
        const auto &events = uuid_entry.value();
        int prev_event = -1;

        for (const auto &event_data : events) {
            Event event;
            event.event_id = event_data["event_id"];
            // event.ftype = stoi(event_data["ftype"]);
            event.ftype = stoi(static_cast<std::string>(event_data["ftype"]));
            event.uuid = event_data["uuid"];
            event.unit_uuid = event_data["unit_uuid"];

            long long event_bytes = sizeof(event);
            cout << "event_bytes: " << event_bytes << endl;
            
            // If ftype == 2, link to previous ftype 1 event in the same UUID
            if (event.ftype == 2 && last_uuid_event.count(event.uuid)) {
                event_parent[event.event_id] = last_uuid_event[event.uuid];
            }

            // Update the last event for this UUID
            if (event.ftype == 1) {
                last_uuid_event[event.uuid] = event.event_id;
            }

            // Link to the previous event in the same unit_uuid
            if (last_unit_uuid_event.count(event.unit_uuid)) {
                event_parent[event.event_id] = last_unit_uuid_event[event.unit_uuid];
            }

            // Update the last event for this unit_uuid
            last_unit_uuid_event[event.unit_uuid] = event.event_id;
        }
    }

    // unit_uuid events
    for (const auto &unit_entry : unit_uuid_data.items()) {
        const string &unit_uuid = unit_entry.key();
        const auto &events = unit_entry.value();
        int prev_event = -1;

        for (const auto &event_data : events) {
            Event event;
            event.event_id = event_data["event_id"];
            event.unit_uuid = event_data["unit_uuid"];

            // Link to the previous event in the same unit_uuid
            if (prev_event != -1) {
                event_parent[event.event_id] = prev_event;
            }

            prev_event = event.event_id;  
        }
    }

    // end time
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Elapsed time: " << elapsed_seconds.count() << "s\n";
    // Print event link chain
    // for (int i = 1; i <= num_events; ++i) {
    //     if (event_parent[i] != -1) {
    //         cout << "Event " << i << " linked to event " << event_parent[i] << endl;
    //     } else {
    //         cout << "Event " << i << " is a root event" << endl;
    //     }
    // }
    
// for (int i = 1; i <= num_events; ++i) {
//     if (event_parent[i] == -1) {
//         // Root event found, now print the chain
//         vector<int> chain;
//         int current_event = i;

//         // Collect all events in the chain
//         while (current_event != -1) {
//             chain.push_back(current_event);
//             current_event = event_parent[current_event];
//         }

//         // Print the event chain in reverse (from root to the most recent)
//         for (int j = chain.size() - 1; j >= 0; --j) {
//             cout << chain[j];
//             if (j > 0) {
//                 cout << " -> ";
//             }
//         }
//         cout << endl;
//     }
// }

}

int main() {
    string uuid_file = "/home/zhaoyinqin/workload/stvr_2/dict_data/traceData1_uuid7.json";
    string unit_uuid_file = "/home/zhaoyinqin/workload/stvr_2/dict_data/traceData1_unit_uuid7.json";

    int num_events = 32273;  
    linkEvents(uuid_file, unit_uuid_file, num_events);

    return 0;
}

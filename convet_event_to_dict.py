import os

def parse_log_line(line):
    data = {}
    pairs = line.strip().split('&')
    for pair in pairs:
        if '=' in pair:
            key, value = pair.split('=', 1)
            data[key] = value
    return data

def process_log_file(file_path):
    uuid_dict = {}
    unit_uuid_dict = {}

    with open(file_path, 'r') as f:
        event_id = 1
        for line in f:
            print(f"Processing line {event_id}, bytes: {len(line)}")
            
            log_entry = parse_log_line(line)
            log_entry['event_id'] = event_id
            
            if 'uuid' in log_entry and log_entry['uuid'] != '':
                uuid = log_entry['uuid']
                if uuid not in uuid_dict:
                    uuid_dict[uuid] = []
                uuid_dict[uuid].append(log_entry)

            elif 'unit_uuid' in log_entry and log_entry['unit_uuid'] != '':
                unit_uuid = log_entry['unit_uuid']
                if unit_uuid not in unit_uuid_dict:
                    unit_uuid_dict[unit_uuid] = []
                unit_uuid_dict[unit_uuid].append(log_entry)

            event_id += 1
        print(f"Processed {event_id} log entries")
    

    return uuid_dict, unit_uuid_dict

# 使用文件路径调用函数来处理数据
for  i in range(1, 11):
    try:
        file_path = f'/home/zhaoyinqin/workload/stvr_2/traceData_10_times/traceData{i}.dat'
        dst_file_path = f'/home/zhaoyinqin/workload/stvr_2/dict_data/traceData1_uuid{i}.json'
        dst_file_path_2 = f'/home/zhaoyinqin/workload/stvr_2/dict_data/traceData1_unit_uuid{i}.json'
        uuid_dict, unit_uuid_dict = process_log_file(file_path)
        # save result to json file
        import json
        with open(dst_file_path, 'w') as f:
            json.dump(uuid_dict, f)
        with open(dst_file_path_2, 'w') as f:
            json.dump(unit_uuid_dict, f)
    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        continue

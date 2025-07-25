#include "disk.h"


Disk::Disk(int disk_id, int disk_capacity, int max_tokens) 
    : id(disk_id)
    , capacity(disk_capacity)
    , head_position(1)
    , storage(disk_capacity + 1, 0)
    , max_tokens_(max_tokens) 
    , partition_size(std::ceil(static_cast<double>(disk_capacity) / DISK_PARTITIONS))
    , token_manager(max_tokens)
    , head_free(true)
    , part_p(nullptr)
    , last_ok(true) {
    // 初始化每个存储单元的分区信息
    storage_partition_map.resize(disk_capacity + 1);        // 存储单元编号从 1 到 disk_capacity
    partitions.resize(DISK_PARTITIONS + 1);                 // 分区编号从 1 到 20
    residual_capacity.resize(DISK_PARTITIONS + 1, 0);       // 分区编号从 1 到 20
    initial_max_capacity.resize(DISK_PARTITIONS + 1, 0);    // 分区编号从 1 到 20

    // 计算区间块的起始索引和大小
    for (int i = 1; i <= DISK_PARTITIONS; i++) {  
        int start = (i - 1) * partition_size + 1;
        int end = std::min(start + partition_size - 1, capacity); 

        // partitions[i] = {start, end - start + 1}; 
        partitions[i] = PartitionInfo(start, end - start + 1);  // 直接初始化 PartitionInfo()
        partitions[i - 1].next = &partitions[i];  // 设置 next 指针
        residual_capacity[i] = end - start + 1;
        initial_max_capacity[i] = end - start + 1;
    }
    partitions[DISK_PARTITIONS].next = &partitions[1];  // 首尾相连

    // 计算存储单元所属的分区
    for (int i = 1; i <= disk_capacity; i++) {
        for (int j = 1; j <= DISK_PARTITIONS; j++) {
            if (i >= partitions[j].start && i < partitions[j].start + partitions[j].size) {
                storage_partition_map[i] = j;  // 直接匹配区间
                break;
            }
        }
        assert(storage_partition_map[i] >= 1 && storage_partition_map[i] <= DISK_PARTITIONS);  // 确保映射合法
    }

    initialize_partitions();
}

// bool Disk::write(int position, int object_id) {
//     assert(position > 0 && position <= capacity);
//     storage[position] = object_id;
//     return true;
// }

// void Disk::erase(int position) {
//     assert(position > 0 && position <= capacity);
//     storage[position] = 0;
// }

// int Disk::get_head_position() const {
//     return head_position;
// }

// bool Disk::is_free(int position) const {
//     assert(position > 0 && position <= capacity);
//     return storage[position] == 0;
// }

// int Disk::get_id() const {
//     return id;
// }

// int Disk::get_capacity() const {
//     return capacity;
// }

// std::vector<int> Disk::get_storage() const{
//     return storage;
// }

// int Disk::get_distance_to_head(int position) const {
//     assert(position > 0 && position <= capacity);
//     if(position < head_position)
//         return capacity - head_position + position;
//     else return position - head_position;
// }


std::pair<int,int> Disk::get_need_token_to_head(int position) const {
    assert(position > 0 && position <= capacity);
    // int distance = get_distance_to_head(position);
    int read_cost = get_need_token_continue_read(position);
    int pass_cost = get_need_token_continue_pass(position);
    int cur_rest_tokens = token_manager.get_current_tokens();
    int cost;
    int action;
    if(read_cost <= pass_cost){
        cost = read_cost;
        action = 0;
    }
    else{
        cost = pass_cost;
        action = 1;
    }
    // 如果是初始阶段 可选择jump
    if(cur_rest_tokens == max_tokens_){
        if(cost < max_tokens_) return {action, cost};
        return {-1, max_tokens_};
    }
    // 如果是中间阶段，不能jump，且过不去，只能jump，那就先尽量随便读，然后下次重新算jump
    if(cost > cur_rest_tokens + max_tokens_){
        return {-2, max_tokens_ + cur_rest_tokens};
    }
    return {action, cost};
}

int Disk::get_need_token_continue_read(int position) const{
    int distance = get_distance_to_head(position);
    // (1+(p2-p1))*readi
    int prev_read_cost_ = token_manager.get_prev_read_cost();
    int cost;
    if(!token_manager.get_last_is_read()){
        prev_read_cost_ = 64;
        cost = prev_read_cost_;
    } else {
        prev_read_cost_ = std::max(16, static_cast<int>(std::ceil(prev_read_cost_ * 0.8)));
        cost = prev_read_cost_;
    }
    for(int i = 1; i <= distance; i++){
        prev_read_cost_ = std::max(16, static_cast<int>(std::ceil(prev_read_cost_ * 0.8)));
        cost += prev_read_cost_;
    }
    return cost;
}

int Disk::get_need_token_continue_pass(int position) const{
    int distance = get_distance_to_head(position);
    int cost = distance + 64;
    return cost;
}

void Disk::refresh_token_manager(){
    token_manager.refresh();
}

int Disk::jump(int position){
    if(token_manager.consume_jump()){
        head_position = position;
        return head_position;
    } else 
        return 0;
}
int Disk::pass(){
    if(token_manager.consume_pass()){
        head_position += 1;
        if(head_position > capacity)
            head_position = 1;
        return head_position;
    }
    else return 0;
}
int Disk::read(){
    if(token_manager.consume_read()){
        head_position += 1;
        if(head_position > capacity)
            head_position = 1;
        return head_position;
    }
    else return 0;
}

// ？？？
// int Disk::get_partition_id(int position) const {
//     assert(position >= 1 && position <= capacity);
//     return storage_partition_map[position];
// }
// int Disk::get_partition_id(int position) const {
//     assert(position > 0 && position <= capacity);
//     return storage_partition_map[position];
// }


// int Disk::get_partition_size() const {
//     return partition_size;
// }

// const PartitionInfo& Disk::get_partition_info(int partition_id) const {
//     assert(partition_id >= 1 && partition_id <= DISK_PARTITIONS);
//     return partitions[partition_id];
// }

void Disk::reflash_partition_score(){
    for (auto& partition : partitions) {
        partition.score = 0.0f;
    }
    initialize_partitions();
}

void Disk::update_partition_head(int part_id, int head){
    partitions[part_id].head_position = head;
}

void Disk::update_partition_info(int partition_id, double score){
    // if (partition_id <= 0 || partition_id > partitions.size() - 1 || score <= 0) return;
    partitions[partition_id].score = score;
    // 调用动态堆 update 操作，调整该分区在堆中的位置
    partition_heap.update(&partitions[partition_id]);


    // partitions[partition_id].score = req.get_size_score() * req.get_time_score();
    // partitions[partition_id].score = req.get_size_score() * req.compute_time_score_update(t);
    // return 0;
}

// const PartitionInfo& Disk::get_partition_info(int partition_id) const {
//     assert(partition_id >= 1 && partition_id <= DISK_PARTITIONS);
//     return partitions[partition_id];
// }

int Disk::get_residual_capacity(int partition_id) const {
    assert(partition_id >= 1 && partition_id <= DISK_PARTITIONS);
    return residual_capacity[partition_id];
}

void Disk::reduce_residual_capacity(int partition_id, int size) {
    assert(partition_id >= 1 && partition_id <= DISK_PARTITIONS);
    assert(size >= 0 && size <= residual_capacity[partition_id]);
    residual_capacity[partition_id] -= size;
    assert(residual_capacity[partition_id] <= initial_max_capacity[partition_id] && "The write operation caused the residual capacity to exceed the maximum capacity");
}

void Disk::increase_residual_capacity(int partition_id, int size) {
    assert(partition_id >= 1 && partition_id <= DISK_PARTITIONS);
    residual_capacity[partition_id] += size;
    // std::cerr<< "-------------------------: "<< (residual_capacity[partition_id] <= initial_max_capacity[partition_id]) << std::endl;
    assert(residual_capacity[partition_id] <= initial_max_capacity[partition_id] && "The deletion operation caused the remaining capacity to exceed the maximum capacity");
}

// bool Disk::head_is_free() const{
//     return head_free;
// }

// void Disk::set_head_busy(){
//     head_free = false;
// }

// void Disk::set_head_free(){
//     head_free = true;
// }

// 新增方法：初始化 partitions 和堆（例如在构造函数中调用）
void Disk::initialize_partitions() {
    // 假定 partitions 的大小已固定为 DISK_PARTITIONS
    // 首先清空现有堆，再将 partitions 中每个元素的地址推入动态堆中
    partition_heap = DynamicPartitionHeap(); // 重置堆
    for (auto it = partitions.begin() + 1; it != partitions.end(); ++it) {
        partition_heap.push(&(*it));
    }
}

// 新增方法：获取堆顶（score 最高）的分区信息
const PartitionInfo* Disk::get_top_partition() {
    return partition_heap.top();
}

// 新增方法：获取堆顶（score 最高）的分区信息
const PartitionInfo* Disk::get_pop_partition() {
    return partition_heap.pop();
}

// int Disk::get_cur_tokens() const {
//     return token_manager.get_current_tokens();
// }

void Disk::push_partition(PartitionInfo* partition) {
    partition_heap.push(partition);
}

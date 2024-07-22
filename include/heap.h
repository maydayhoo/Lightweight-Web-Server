#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/types.h>

template<typename DataType, typename Comparator = std::greater<int>>
class Heap {
    using const_reference = const DataType&;
public:
    Heap() { std::cout << "yes" << std::endl; }
    void insert(DataType _elem) {

        _elem_container.push_back(std::move(_elem));

        _adjust_bottom_up(_elem_container.size() - 1);
    }

    const_reference top() {
        return _elem_container.front();
    }

    template<typename Predicate>
    void modify(const_reference elem, Predicate pred) {

        auto iter = std::find_if(_elem_container.begin(), _elem_container.end(), pred);

        if (iter == _elem_container.end()) return;

        _adjust_top_down(iter - _elem_container.begin());
    }

private:
    std::vector<DataType> _elem_container;
    Comparator comparator;
private:

    // 自下而上的调整
    void _adjust_bottom_up(int idx) {

        int root_idx = idx / 2;
        while (idx != 0 && comparator(_elem_container[root_idx], _elem_container[idx])) {
            std::swap(_elem_container[root_idx], _elem_container[idx]);
            idx = root_idx;
        }
    }

    // 自上而下的调整
    void _adjust_top_down(int idx) {

        int target_idx = -1;

        while (idx < _elem_container.size()) {

            int left_child_idx = (2 * idx < _elem_container.size() ?
                2 * idx : -1);
            int right_child_idx = (2 * idx + 1 < _elem_container.size() ?
                2 * idx + 1 : -1);

            if (left_child_idx != -1 && \
                comparator(_elem_container[left_child_idx], _elem_container[idx])) {

                target_idx = left_child_idx;
            }

            if (right_child_idx != -1 &&
                (target_idx == -1 || comparator(_elem_container[right_child_idx], _elem_container[target_idx]))) {

                target_idx = right_child_idx;
            }

            if (target_idx == -1) break;

            std::swap(_elem_container[target_idx], _elem_container[idx]);
            idx = target_idx;
        }

    }
};

template<typename Comparator>
class Heap<std::pair<int, int>, Comparator> {

    using const_reference = const std::pair<int, int>&;
private:
    int _heap_sz = 0;
public:
    Heap(Comparator _comparator):comparator(_comparator) { 
        std::cout << "partial one" << std::endl; 
    }
    using DataType = std::pair<int, int>;
    static const int BEG_IDX = 0;
public:

    // 清空堆
    void clear() {

        _heap_sz = 0;
        _elem_container.clear();
    }

    // 堆排序 - 排序后，堆将被破坏
    void sort() {

        while (_heap_sz > 0) {
            std::swap(_elem_container[0], _elem_container[_heap_sz - 1]);
            --_heap_sz;
            _adjust_top_down(0);
        }
    }

    void print_heap() {
        for_each(_elem_container.begin(), _elem_container.end(), [](std::pair<int, int> p) {
            std::cout << p.second << " ";
            });
        std::cout << std::endl;
    }

    void insert(DataType _elem) {

        ++_heap_sz;
        _elem_container.push_back(std::move(_elem));

        _adjust_bottom_up(_elem_container.size() - 1);
    }

    const_reference top() {

        return _elem_container.front();
    }

    template<typename Predicate>
    void delete_from_heap(const_reference elem, Predicate pred) {

        auto iter = std::find_if(_elem_container.begin(), _elem_container.end(), pred);
        if (iter == _elem_container.end()) {

            return;
        }

        std::swap(*iter, _elem_container[_heap_sz - 1]);

        --_heap_sz;

        _adjust_top_down(iter - _elem_container.begin());
    }

    template<typename Predicate>
    void modify(const_reference elem, Predicate pred) {

        auto iter = std::find_if(_elem_container.begin(), _elem_container.begin() + _heap_sz, pred);

        if (iter == _elem_container.begin() + _heap_sz) return;

        if (comparator(elem.second, iter->second)) {

            iter->second = elem.second;
            _adjust_bottom_up(iter - _elem_container.begin());
        }
        else {
            iter->second = elem.second;
            _adjust_top_down(iter - _elem_container.begin(), _heap_sz);
        }
    }
private:

    std::vector<DataType> _elem_container;
    Comparator comparator;
private:

    // 自下而上的调整
    void _adjust_bottom_up(int idx) {

        int root_idx = idx / 2;
        while (idx != BEG_IDX && \
            comparator(_elem_container[idx].second, _elem_container[root_idx].second)) {

            std::swap(_elem_container[idx], _elem_container[root_idx]);
            idx = root_idx;
            root_idx = idx / 2;
        }
    }

    // 自上而下的调整
    void _adjust_top_down(int i) {

        while (i != -1 && i < _heap_sz) {

            int left_child_idx = i * 2 + 1;
            int right_child_idx = i * 2 + 2;
            int target_idx = -1;

            if (left_child_idx < _heap_sz && comparator(_elem_container[left_child_idx].second, _elem_container[i].second)) {
                target_idx = left_child_idx;
            }
            if (right_child_idx < _heap_sz && (target_idx == -1 || comparator(_elem_container[right_child_idx].second, _elem_container[target_idx].second))) {
                target_idx = right_child_idx;
            }

            i = (target_idx == -1) ? -1 : (std::swap(_elem_container[target_idx], _elem_container[i]), target_idx);
        }
    }
};
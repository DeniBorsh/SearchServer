#pragma once

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket;

public:
    static_assert(std::is_integral_v<Key>);

    struct Access {
        std::lock_guard <std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex),
            ref_to_value(bucket.std::map[key])
        {
        }
    };

    explicit Concurrentstd::map(size_t bucket_count)
    {
        buckets_ = new Bucket[bucket_count];
        buckets_size_ = bucket_count;
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[key % buckets_size_];
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinarystd::map() {
        std::map<Key, Value> result;
        for (int i = 0; i < buckets_size_; ++i) {
            std::lock_guard guard(buckets_[i].mutex);
            result.insert(buckets_[i].std::map.begin(), buckets_[i].std::map.end());
        }
        return result;
    }

private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
    size_t buckets_size_;
    Bucket* buckets_;
};
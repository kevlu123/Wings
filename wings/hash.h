#pragma once
#include <stdint.h>
#include <optional>
#include <vector>
#include <stdexcept>
#include <utility>

/*
* The RelaxedSet and RelaxedMap are versions of std::unordered_set
* and std::unordered_map with more relaxed requirements.
* 
* Unlike the STL versions, an inconsistent hash or equality
* function will yield unspecified behaviour instead of
* undefined behaviour.
* Furthermore, the container can be modified while iterating
* through it. Doing so will yield unspecified but not undefined behaviour.
* 
* RelaxedMap will also iterate by insertion order. To achieve fast ordered iteration
* and O(1) insertion, deletion, and lookup, deletions do not shrink the
* underlying buffer.
* 
* If an exception is thrown from the hash or equality function,
* the container if left unmodified.
*/

namespace wings {

	template <class Key, class Hash, class Equal, class BucketItem>
	struct RelaxedHash {
	private:
		using Bucket = std::vector<BucketItem>;
		
	public:
		virtual void rehash(size_t count) = 0;
		
		RelaxedHash() : hasher(), equal(), buckets(1) {
		}

		bool contains(const Key& key) const {
			const Bucket* buck = nullptr;
			return get_item(key, &buck);
		}

		bool empty() const noexcept {
			return size() == 0;
		}

		size_t size() const noexcept {
			return mySize;
		}

		size_t bucket_count() const noexcept {
			return buckets.size();
		}

		size_t bucket(const Key& key) const {
			return hasher(key) % bucket_count();
		}

		size_t bucket_size(size_t n) noexcept {
			return buckets[n].size();
		}

		float load_factor() const noexcept {
			return (float)size() / bucket_count();
		}

		float max_load_factor() const noexcept {
			return maxLoadFactor;
		}

		void max_load_factor(float ml) noexcept {
			maxLoadFactor = ml;
		}

	protected:
		virtual BucketItem* get_item(const Key& key, Bucket** buck) = 0;
		virtual const BucketItem* get_item(const Key& key, const Bucket** buck) const = 0;
		
		void incr_size() {
			mySize++;
			if (load_factor() > max_load_factor())
				rehash(bucket_count() * 2 + 1);
		}

		void decr_size() {
			mySize--;
		}

		void clear_buckets() noexcept {
			for (auto& buck : buckets)
				buck.clear();
			mySize = 0;
		}
		
		Hash hasher;
		Equal equal;
		std::vector<Bucket> buckets;
		float maxLoadFactor = 1.0f;
		size_t mySize = 0;
	};

	template <class Key, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
	struct RelaxedSet : RelaxedHash<Key, Hash, Equal, Key> {
	private:
		template <class Container>
		struct Iterator {
			Iterator(Container* container = nullptr, size_t bucketIndex = (size_t)-1, size_t itemIndex = (size_t)-1) :
				container(container), bucketIndex(bucketIndex), itemIndex(itemIndex) {
				Revalidate();
			}
			
			const Key& operator*() const {
				return container->buckets[bucketIndex][itemIndex];
			}

			const Key* operator->() const {
				return &container->buckets[bucketIndex][itemIndex];
			}

			Iterator& operator++() {
				itemIndex++;
				Revalidate();
				return *this;
			}

			bool operator==(const Iterator& rhs) const {
				return (!container && !rhs.container)
					|| (bucketIndex == rhs.bucketIndex
					    && itemIndex == rhs.itemIndex);
			}

			bool operator!=(const Iterator& rhs) const {
				return !(*this == rhs);
			}

			void Revalidate() {
				while (!CheckEnd() && itemIndex >= container->buckets[bucketIndex].size()) {
					bucketIndex++;
					itemIndex = 0;
				}
			}
		private:
			bool CheckEnd() {
				if (container && bucketIndex >= container->buckets.size())
					container = nullptr;
				return container == nullptr;
			}
			
			friend RelaxedSet;
			Container* container;
			size_t bucketIndex;
			size_t itemIndex;
		};
		
		using Bucket = std::vector<Key>;

	public:
		using iterator = Iterator<RelaxedSet>;
		using const_iterator = Iterator<const RelaxedSet>;

		void clear() noexcept {
			this->clear_buckets();
		}
		
		void insert(Key key) {
			Bucket* buck = nullptr;
			if (!this->get_item(key, &buck)) {
				buck->push_back(std::move(key));
				this->incr_size();
			}
		}

		const_iterator find(const Key& key) const {
			const Bucket* buck = nullptr;
			if (auto* item = this->get_item(key, &buck)) {
				return const_iterator{
					this,
					(size_t)(buck - this->buckets.data()),
					(size_t)(item - buck->data())
				};
			}
			return end();
		}

		void erase(iterator it) {
			auto& buck = this->buckets[it.bucketIndex];
			buck.erase(buck.begin() + it.itemIndex);
			this->decr_size();
		}

		void erase(const_iterator it) {
			erase(iterator{ this, it.bucketIndex, it.itemIndex });
		}

		void rehash(size_t count) override {
			auto oldBuckets = std::move(this->buckets);
			clear();
			this->buckets.resize(count);
			for (auto& oldBucket : oldBuckets) {
				for (auto& item : oldBucket) {
					Bucket* buck = nullptr;
					this->get_item(item, &buck);
					buck->push_back(std::move(item));
					this->mySize++;
				}
			}
		}

		const_iterator cbegin() const noexcept { return const_iterator(this, 0, 0); }
		const_iterator begin() const noexcept { return cbegin(); }
		iterator begin() noexcept { return iterator(this, 0, 0); }
		const_iterator cend() const noexcept { return const_iterator(); }
		const_iterator end() const noexcept { return cend(); }
		iterator end() noexcept { return iterator(); }
		
	protected:
		Key* get_item(const Key& key, Bucket** buck) override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& item : b)
				if (this->equal(item, key))
					return &item;
			return nullptr;
		}

		const Key* get_item(const Key& key, const Bucket** buck) const override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& item : b)
				if (this->equal(item, key))
					return &item;
			return nullptr;
		}
	};
	
	template <class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
	struct RelaxedMap : RelaxedHash<Key, Hash, Equal, size_t> {
	private:
		template <class Container>
		struct Iterator {
			Iterator(Container* container = nullptr, size_t index = (size_t)-1) :
				container(container), index(index) {
				Revalidate();
			}
			
			auto& operator*() const {
				return container->storage[index].value();
			}

			auto* operator->() const {
				return &container->storage[index].value();
			}

			Iterator& operator++() {
				index++;
				Revalidate();
				return *this;
			}

			bool operator==(const Iterator& rhs) const {
				return (!container && !rhs.container) || index == rhs.index;
			}

			bool operator!=(const Iterator& rhs) const {
				return !(*this == rhs);
			}

			void Revalidate() {
				while (!CheckEnd() && container->storage[index] == std::nullopt) {
					index++;
				}
			}
		private:
			bool CheckEnd() {
				if (container && index >= container->storage.size())
					container = nullptr;
				return container == nullptr;
			}

			friend RelaxedMap;
			Container* container;
			size_t index;
		};

		using Bucket = std::vector<size_t>;
		std::vector<std::optional<std::pair<const Key, Value>>> storage;
		
	public:
		using iterator = Iterator<RelaxedMap>;
		using const_iterator = Iterator<const RelaxedMap>;

		RelaxedMap() = default;
		RelaxedMap(RelaxedMap&&) = delete;
		RelaxedMap& operator=(RelaxedMap&&) = delete;

		void clear() noexcept {
			this->clear_buckets();
			storage.clear();
		}
		
		void insert(std::pair<const Key, Value> pair) {
			Bucket* buck = nullptr;
			if (!this->get_item(pair.first, &buck)) {
				buck->push_back(storage.size());
				storage.push_back(std::move(pair));
				this->incr_size();
			}
		}

		std::optional<Value> erase(const Key& key) {
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck)) {
				buck->erase(std::find(buck->begin(), buck->end(), *index));
				auto pair = std::move(storage[*index]);
				storage[*index] = std::nullopt;
				this->decr_size();
				return pair.value().second;
			}
			return std::nullopt;
		}

		std::pair<const Key, Value> pop() {
			size_t i = storage.size() - 1;
			while (storage[i] == std::nullopt)
				i--;
			const auto& key = storage[i].value().first;

			Bucket* buck = nullptr;
			auto* index = this->get_item(key, &buck);
			buck->erase(std::find(buck->begin(), buck->end(), *index));
			auto pair = std::move(storage[*index]);
			storage[*index] = std::nullopt;
			this->decr_size();
			return pair.value();
		}
		
		iterator find(const Key& key) {
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return iterator{ this, *index };
			return end();
		}

		const_iterator find(const Key& key) const {
			const Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return iterator{ this, *index };
			return end();
		}

		Value& at(const Key& key) {
			if (auto* value = try_at(key))
				return *value;
			throw std::out_of_range();
		}
		
		const Value& at(const Key& key) const {
			if (auto* value = try_at(key))
				return *value;
			throw std::out_of_range();
		}

		Value& operator[](const Key& key) {
			this->incr_size();
			Bucket* buck = nullptr;
			if (auto* index = this->get_item(key, &buck))
				return storage[*index].value().second;
			buck->push_back(storage.size());
			storage.push_back(std::pair<const Key, Value>({ key, Value() }));
			return storage.back().value().second;
		}

		void rehash(size_t count) override {
			this->buckets.clear();
			this->buckets.resize(count);
			for (size_t i = 0; i < storage.size(); i++) {
				if (storage[i] != std::nullopt) {
					Bucket* buck = nullptr;
					this->get_item(storage[i].value().first, &buck);
					buck->push_back(i);
				}
			}
		}

		const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
		const_iterator begin() const noexcept { return cbegin(); }
		iterator begin() noexcept { return iterator(this, 0); }
		const_iterator cend() const noexcept { return const_iterator(); }
		const_iterator end() const noexcept { return cend(); }
		iterator end() noexcept { return iterator(); }

	protected:
		size_t* get_item(const Key& key, Bucket** buck) override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& index : b)
				if (this->equal(storage[index].value().first, key))
					return &index;
			return nullptr;
		}

		const size_t* get_item(const Key& key, const Bucket** buck) const override {
			auto& b = this->buckets[this->bucket(key)];
			*buck = &b;
			for (auto& index : b)
				if (this->equal(storage[index].value().first, key))
					return &index;
			return nullptr;
		}
	};
}

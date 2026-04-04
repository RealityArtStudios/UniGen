// Copyright (c) CreationArtStudios

#pragma once
#include <cstdint>
#include <vector>
#include <bitset>
#include <memory>
#include <algorithm>
#include <limits>

using EntityID = uint64_t;
using EntityIndex = uint32_t;
using EntityVersion = uint32_t;
constexpr size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

constexpr EntityID INVALID_ENTITY = UINT64_MAX;

inline EntityID CreateEntityId(EntityIndex index, EntityVersion version)
{
    return (static_cast<EntityID>(index) << 32) | static_cast<EntityID>(version);
}

inline EntityIndex GetEntityIndex(EntityID id)
{
    return static_cast<EntityIndex>(id >> 32);
}

inline EntityVersion GetEntityVersion(EntityID id)
{
    return static_cast<EntityVersion>(id);
}

inline bool IsEntityValid(EntityID id)
{
    return GetEntityIndex(id) != std::numeric_limits<EntityIndex>::max();
}

inline int& GetComponentIdCounter()
{
    static int counter = 0;
    return counter;
}

template<typename T>
int GetComponentId()
{
    static int id = GetComponentIdCounter()++;
    return id;
}

class ComponentPool
{
public:
    ComponentPool() = default;
    explicit ComponentPool(size_t elementSize, size_t initialCapacity = 100)
        : ElementSize(elementSize)
    {
        Data.resize(elementSize * initialCapacity);
    }

    void* Get(size_t index)
    {
        return Data.data() + index * ElementSize;
    }

    const void* Get(size_t index) const
    {
        return Data.data() + index * ElementSize;
    }

    void Reserve(size_t capacity)
    {
        if (capacity * ElementSize > Data.size())
        {
            Data.resize(ElementSize * capacity);
        }
    }

    size_t Size() const { return Data.size() / ElementSize; }
    size_t ElementSize = 0;
    std::vector<char> Data;
};

class Scene
{
public:
    struct EntityDesc
    {
        EntityID id;
        ComponentMask mask;
    };

    Scene() = default;

    EntityID CreateEntity()
    {
        if (!FreeIndices.empty())
        {
            EntityIndex newIndex = FreeIndices.back();
            FreeIndices.pop_back();
            EntityID newId = CreateEntityId(newIndex, GetEntityVersion(Entities[newIndex].id));
            Entities[newIndex].id = newId;
            Entities[newIndex].mask.reset();
            return newId;
        }

        EntityIndex newIndex = static_cast<EntityIndex>(Entities.size());
        EntityID newId = CreateEntityId(newIndex, 0);
        Entities.push_back({ newId, ComponentMask() });
        return newId;
    }

    void DestroyEntity(EntityID id)
    {
        EntityIndex index = GetEntityIndex(id);
        if (index >= Entities.size())
            return;
        if (Entities[index].id != id)
            return;

        Entities[index].mask.reset();
        EntityID newId = CreateEntityId(std::numeric_limits<EntityIndex>::max(), GetEntityVersion(id) + 1);
        Entities[index].id = newId;
        FreeIndices.push_back(index);
    }

    bool IsEntityValid(EntityID id) const
    {
        EntityIndex index = GetEntityIndex(id);
        if (index >= Entities.size())
            return false;
        return Entities[index].id == id;
    }

    template<typename T>
    bool HasComponent(EntityID id) const
    {
        EntityIndex index = GetEntityIndex(id);
        if (!IsEntityValid(id))
            return false;
        return Entities[index].mask.test(GetComponentId<T>());
    }

    template<typename T>
    T* GetComponent(EntityID id)
    {
        EntityIndex index = GetEntityIndex(id);
        if (!IsEntityValid(id))
            return nullptr;
        if (!Entities[index].mask.test(GetComponentId<T>()))
            return nullptr;

        int componentId = GetComponentId<T>();
        if (componentId >= static_cast<int>(ComponentPools.size()))
            return nullptr;
        if (!ComponentPools[componentId])
            return nullptr;

        return static_cast<T*>(ComponentPools[componentId]->Get(index));
    }

    template<typename T>
    T* AddComponent(EntityID id)
    {
        EntityIndex index = GetEntityIndex(id);
        if (!IsEntityValid(id))
            return nullptr;

        int componentId = GetComponentId<T>();
        if (componentId >= static_cast<int>(ComponentPools.size()))
        {
            ComponentPools.resize(componentId + 1);
        }

        if (!ComponentPools[componentId])
        {
            ComponentPools[componentId] = std::make_unique<ComponentPool>(sizeof(T), Entities.size());
        }
        else
        {
            ComponentPools[componentId]->Reserve(Entities.size());
        }

        Entities[index].mask.set(componentId);
        T* component = new (ComponentPools[componentId]->Get(index)) T();
        return component;
    }

    template<typename T>
    void RemoveComponent(EntityID id)
    {
        EntityIndex index = GetEntityIndex(id);
        if (!IsEntityValid(id))
            return;

        int componentId = GetComponentId<T>();
        Entities[index].mask.reset(componentId);
    }

    template<typename... ComponentTypes>
    class View
    {
    public:
        class Iterator
        {
        public:
            Iterator(Scene* scene, EntityIndex index, ComponentMask mask)
                : ScenePtr(scene), Index(index), Mask(mask) {}

            EntityID operator*() const
            {
                return ScenePtr->Entities[Index].id;
            }

            bool operator==(const Iterator& other) const
            {
                return Index == other.Index || Index == ScenePtr->Entities.size();
            }

            bool operator!=(const Iterator& other) const
            {
                return !(*this == other);
            }

            Iterator& operator++()
            {
                do
                {
                    Index++;
                } while (Index < ScenePtr->Entities.size() && !Valid());
                return *this;
            }

        private:
            bool Valid() const
            {
                if (!ScenePtr->IsEntityValid(ScenePtr->Entities[Index].id))
                    return false;
                return (Mask & ScenePtr->Entities[Index].mask) == Mask;
            }

            Scene* ScenePtr;
            EntityIndex Index;
            ComponentMask Mask;
        };

        View(Scene* scene) : ScenePtr(scene)
        {
            if constexpr (sizeof...(ComponentTypes) > 0)
            {
                int ids[] = { 0, GetComponentId<ComponentTypes>()... };
                for (int i = 1; i <= static_cast<int>(sizeof...(ComponentTypes)); i++)
                {
                    Mask.set(ids[i]);
                }
            }
        }

        Iterator begin()
        {
            EntityIndex start = 0;
            while (start < ScenePtr->Entities.size())
            {
                if (ScenePtr->IsEntityValid(ScenePtr->Entities[start].id) &&
                    (Mask & ScenePtr->Entities[start].mask) == Mask)
                {
                    break;
                }
                start++;
            }
            return Iterator(ScenePtr, start, Mask);
        }

        Iterator end()
        {
            return Iterator(ScenePtr, static_cast<EntityIndex>(ScenePtr->Entities.size()), Mask);
        }

    private:
        Scene* ScenePtr;
        ComponentMask Mask;
    };

    template<typename... ComponentTypes>
    View<ComponentTypes...> CreateView()
    {
        return View<ComponentTypes...>(this);
    }

    size_t GetEntityCount() const { return Entities.size(); }

private:
    std::vector<EntityDesc> Entities;
    std::vector<EntityIndex> FreeIndices;
    std::vector<std::unique_ptr<ComponentPool>> ComponentPools;
};

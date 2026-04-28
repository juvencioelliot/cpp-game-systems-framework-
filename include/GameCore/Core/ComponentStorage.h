#pragma once

#include "GameCore/Core/Entity.h"

#include <cstddef>
#include <unordered_map>

namespace GameCore::Core
{
    class IComponentStorage
    {
    public:
        virtual ~IComponentStorage() = default;

        virtual void remove(EntityID entity) = 0;
    };

    template <typename Component>
    class ComponentStorage final : public IComponentStorage
    {
    public:
        void add(EntityID entity, const Component& component)
        {
            m_components[entity] = component;
        }

        void remove(EntityID entity) override
        {
            m_components.erase(entity);
        }

        void clear()
        {
            m_components.clear();
        }

        [[nodiscard]] bool has(EntityID entity) const
        {
            return m_components.find(entity) != m_components.end();
        }

        Component* get(EntityID entity)
        {
            auto it = m_components.find(entity);
            return it == m_components.end() ? nullptr : &it->second;
        }

        const Component* get(EntityID entity) const
        {
            auto it = m_components.find(entity);
            return it == m_components.end() ? nullptr : &it->second;
        }

        [[nodiscard]] const std::unordered_map<EntityID, Component>& all() const
        {
            return m_components;
        }

        [[nodiscard]] std::unordered_map<EntityID, Component>& all()
        {
            return m_components;
        }

        [[nodiscard]] std::size_t size() const
        {
            return m_components.size();
        }

    private:
        std::unordered_map<EntityID, Component> m_components;
    };
}

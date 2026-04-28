#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace GameCore::Core
{
    class EventBus
    {
    public:
        using ListenerID = std::size_t;

        template <typename Event>
        ListenerID subscribe(std::function<void(const Event&)> listener)
        {
            if (!listener)
            {
                throw std::invalid_argument("Event listener cannot be empty.");
            }

            auto& listeners = storage<Event>();
            const ListenerID id = m_nextListenerID++;

            listeners.push_back(TypedListener<Event>{
                id,
                std::move(listener),
            });

            return id;
        }

        template <typename Event>
        void unsubscribe(ListenerID id)
        {
            auto* listeners = findStorage<Event>();
            if (listeners == nullptr)
            {
                return;
            }

            listeners->erase(
                std::remove_if(listeners->begin(),
                               listeners->end(),
                               [id](const TypedListener<Event>& listener) {
                                   return listener.id == id;
                               }),
                listeners->end());
        }

        template <typename Event>
        void publish(const Event& event) const
        {
            const auto* listeners = findStorage<Event>();
            if (listeners == nullptr)
            {
                return;
            }

            const auto listenersSnapshot = *listeners;
            for (const auto& listener : listenersSnapshot)
            {
                listener.callback(event);
            }
        }

        template <typename Event>
        void clear()
        {
            auto* listeners = findStorage<Event>();
            if (listeners != nullptr)
            {
                listeners->clear();
            }
        }

        void clearAll()
        {
            m_listenerStorage.clear();
        }

        template <typename Event>
        [[nodiscard]] std::size_t listenerCount() const
        {
            const auto* listeners = findStorage<Event>();
            return listeners == nullptr ? 0 : listeners->size();
        }

    private:
        class IListenerStorage
        {
        public:
            virtual ~IListenerStorage() = default;
        };

        template <typename Event>
        struct TypedListener
        {
            ListenerID id{0};
            std::function<void(const Event&)> callback;
        };

        template <typename Event>
        class ListenerStorage final : public IListenerStorage
        {
        public:
            std::vector<TypedListener<Event>> listeners;
        };

        template <typename Event>
        std::vector<TypedListener<Event>>& storage()
        {
            const std::type_index key{typeid(Event)};
            auto it = m_listenerStorage.find(key);

            if (it == m_listenerStorage.end())
            {
                auto inserted = m_listenerStorage.emplace(
                    key,
                    std::make_unique<ListenerStorage<Event>>());
                it = inserted.first;
            }

            return static_cast<ListenerStorage<Event>&>(*it->second).listeners;
        }

        template <typename Event>
        std::vector<TypedListener<Event>>* findStorage()
        {
            const std::type_index key{typeid(Event)};
            auto it = m_listenerStorage.find(key);
            if (it == m_listenerStorage.end())
            {
                return nullptr;
            }

            return &static_cast<ListenerStorage<Event>&>(*it->second).listeners;
        }

        template <typename Event>
        const std::vector<TypedListener<Event>>* findStorage() const
        {
            const std::type_index key{typeid(Event)};
            auto it = m_listenerStorage.find(key);
            if (it == m_listenerStorage.end())
            {
                return nullptr;
            }

            return &static_cast<const ListenerStorage<Event>&>(*it->second).listeners;
        }

        ListenerID m_nextListenerID{1};
        std::unordered_map<std::type_index, std::unique_ptr<IListenerStorage>> m_listenerStorage;
    };
}

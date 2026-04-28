#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace GameCore::Core
{
    template <typename Resource>
    class ResourceHandle
    {
    public:
        ResourceHandle() = default;

        ResourceHandle(std::string id, std::shared_ptr<Resource> resource)
            : m_id(std::move(id)),
              m_resource(std::move(resource))
        {
        }

        [[nodiscard]] bool isValid() const
        {
            return m_resource != nullptr;
        }

        [[nodiscard]] const std::string& id() const
        {
            return m_id;
        }

        [[nodiscard]] Resource* get() const
        {
            return m_resource.get();
        }

        [[nodiscard]] const std::shared_ptr<Resource>& shared() const
        {
            return m_resource;
        }

        [[nodiscard]] long useCount() const
        {
            return m_resource.use_count();
        }

        Resource& operator*() const
        {
            return *m_resource;
        }

        Resource* operator->() const
        {
            return m_resource.get();
        }

    private:
        std::string m_id;
        std::shared_ptr<Resource> m_resource;
    };

    class ResourceManager
    {
    public:
        template <typename Resource>
        using Loader = std::function<std::shared_ptr<Resource>()>;

        template <typename Resource>
        ResourceHandle<Resource> load(const std::string& id, Loader<Resource> loader)
        {
            validateID(id);
            if (!loader)
            {
                throw std::invalid_argument("Resource loader cannot be empty.");
            }

            auto& resourceStorage = storage<Resource>();
            auto it = resourceStorage.resources.find(id);
            if (it != resourceStorage.resources.end())
            {
                return ResourceHandle<Resource>(id, it->second.resource);
            }

            auto resource = loader();
            if (resource == nullptr)
            {
                throw std::runtime_error("Resource loader returned null.");
            }

            resourceStorage.resources.emplace(id, ResourceRecord<Resource>{
                                                      resource,
                                                      std::move(loader),
                                                  });

            return ResourceHandle<Resource>(id, std::move(resource));
        }

        template <typename Resource, typename Factory>
        ResourceHandle<Resource> loadValue(const std::string& id, Factory factory)
        {
            return load<Resource>(id, [factory = std::move(factory)]() mutable {
                return std::make_shared<Resource>(factory());
            });
        }

        template <typename Resource>
        [[nodiscard]] ResourceHandle<Resource> get(const std::string& id) const
        {
            const auto* resourceStorage = findStorage<Resource>();
            if (resourceStorage == nullptr)
            {
                return {};
            }

            const auto it = resourceStorage->resources.find(id);
            if (it == resourceStorage->resources.end())
            {
                return {};
            }

            return ResourceHandle<Resource>(id, it->second.resource);
        }

        template <typename Resource>
        [[nodiscard]] ResourceHandle<Resource> require(const std::string& id) const
        {
            auto handle = get<Resource>(id);
            if (!handle.isValid())
            {
                throw std::runtime_error("Required resource is not loaded.");
            }

            return handle;
        }

        template <typename Resource>
        [[nodiscard]] bool has(const std::string& id) const
        {
            return get<Resource>(id).isValid();
        }

        template <typename Resource>
        bool reload(const std::string& id)
        {
            auto* resourceStorage = findStorage<Resource>();
            if (resourceStorage == nullptr)
            {
                return false;
            }

            auto it = resourceStorage->resources.find(id);
            if (it == resourceStorage->resources.end())
            {
                return false;
            }

            auto resource = it->second.loader();
            if (resource == nullptr)
            {
                throw std::runtime_error("Resource loader returned null during reload.");
            }

            it->second.resource = std::move(resource);
            return true;
        }

        template <typename Resource>
        void unload(const std::string& id)
        {
            auto* resourceStorage = findStorage<Resource>();
            if (resourceStorage != nullptr)
            {
                resourceStorage->resources.erase(id);
            }
        }

        template <typename Resource>
        void unloadAll()
        {
            auto* resourceStorage = findStorage<Resource>();
            if (resourceStorage != nullptr)
            {
                resourceStorage->resources.clear();
            }
        }

        void clearAll()
        {
            m_resourceStorage.clear();
        }

        template <typename Resource>
        [[nodiscard]] std::size_t resourceCount() const
        {
            const auto* resourceStorage = findStorage<Resource>();
            return resourceStorage == nullptr ? 0 : resourceStorage->resources.size();
        }

        [[nodiscard]] std::size_t resourceTypeCount() const
        {
            return m_resourceStorage.size();
        }

    private:
        class IResourceStorage
        {
        public:
            virtual ~IResourceStorage() = default;
        };

        template <typename Resource>
        struct ResourceRecord
        {
            std::shared_ptr<Resource> resource;
            Loader<Resource> loader;
        };

        template <typename Resource>
        class TypedResourceStorage final : public IResourceStorage
        {
        public:
            std::unordered_map<std::string, ResourceRecord<Resource>> resources;
        };

        static void validateID(const std::string& id)
        {
            if (id.empty())
            {
                throw std::invalid_argument("Resource id cannot be empty.");
            }
        }

        template <typename Resource>
        TypedResourceStorage<Resource>& storage()
        {
            const std::type_index key{typeid(Resource)};
            auto it = m_resourceStorage.find(key);

            if (it == m_resourceStorage.end())
            {
                auto inserted = m_resourceStorage.emplace(
                    key,
                    std::make_unique<TypedResourceStorage<Resource>>());
                it = inserted.first;
            }

            return static_cast<TypedResourceStorage<Resource>&>(*it->second);
        }

        template <typename Resource>
        TypedResourceStorage<Resource>* findStorage()
        {
            const std::type_index key{typeid(Resource)};
            auto it = m_resourceStorage.find(key);
            if (it == m_resourceStorage.end())
            {
                return nullptr;
            }

            return static_cast<TypedResourceStorage<Resource>*>(it->second.get());
        }

        template <typename Resource>
        const TypedResourceStorage<Resource>* findStorage() const
        {
            const std::type_index key{typeid(Resource)};
            auto it = m_resourceStorage.find(key);
            if (it == m_resourceStorage.end())
            {
                return nullptr;
            }

            return static_cast<const TypedResourceStorage<Resource>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IResourceStorage>> m_resourceStorage;
    };
}

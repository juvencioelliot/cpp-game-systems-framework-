#include "GameCore/Systems/TransformSystem.h"

#include "GameCore/Components/TransformComponent.h"
#include "GameCore/Core/World.h"

#include <unordered_set>
#include <vector>

namespace GameCore::Systems
{
    namespace
    {
        struct TransformResolveResult
        {
            Components::WorldTransformComponent transform;
            bool validParentChain{true};
        };

        TransformResolveResult resolveTransform(Core::World& world,
                                                Core::EntityID entity,
                                                std::unordered_set<Core::EntityID>& resolving)
        {
            const auto* transform = world.getComponent<Components::TransformComponent>(entity);
            if (transform == nullptr)
            {
                return {};
            }

            if (resolving.find(entity) != resolving.end())
            {
                return {
                    Components::WorldTransformComponent{
                        transform->localX,
                        transform->localY,
                        transform->rotationRadians,
                        transform->scaleX,
                        transform->scaleY,
                    },
                    false,
                };
            }

            resolving.insert(entity);

            TransformResolveResult result{
                Components::WorldTransformComponent{
                    transform->localX,
                    transform->localY,
                    transform->rotationRadians,
                    transform->scaleX,
                    transform->scaleY,
                },
                true,
            };

            if (transform->parent != Core::InvalidEntity && world.isAlive(transform->parent))
            {
                const auto parent = resolveTransform(world, transform->parent, resolving);
                if (parent.validParentChain)
                {
                    result.transform.x = parent.transform.x + transform->localX;
                    result.transform.y = parent.transform.y + transform->localY;
                    result.transform.rotationRadians =
                        parent.transform.rotationRadians + transform->rotationRadians;
                    result.transform.scaleX = parent.transform.scaleX * transform->scaleX;
                    result.transform.scaleY = parent.transform.scaleY * transform->scaleY;
                }
                else
                {
                    result.validParentChain = false;
                }
            }

            auto* existing = world.getComponent<Components::WorldTransformComponent>(entity);
            if (existing != nullptr)
            {
                *existing = result.transform;
            }
            else
            {
                world.addComponent(entity, result.transform);
            }

            resolving.erase(entity);
            return result;
        }
    }

    void TransformSystem::update(Core::World& world, const Core::FrameContext&)
    {
        std::vector<Core::EntityID> entities;
        world.each<Components::TransformComponent>(
            [&entities](Core::EntityID entity, const Components::TransformComponent&) {
                entities.push_back(entity);
            });

        std::unordered_set<Core::EntityID> resolving;
        for (const auto entity : entities)
        {
            resolveTransform(world, entity, resolving);
        }
    }
}

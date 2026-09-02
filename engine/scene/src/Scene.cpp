#include "AREngine/Scene/Scene.hpp"

#include "AREngine/Core/Assert.hpp"

#include <algorithm>

namespace AREngine::Scene
{
    EntityId Scene::CreateEntity(std::string name)
    {
        const EntityId id{m_nextEntityId++};

        EntityRecord record;
        record.name = std::move(name);
        m_entities.emplace(id, std::move(record));

        return id;
    }

    void Scene::DestroyEntity(EntityId entity)
    {
        if (!IsValid(entity))
        {
            return;
        }

        // Recursively destroy descendants first, so no child is ever
        // left referencing a destroyed parent. Copy the children list
        // before recursing: the recursive calls below erase from
        // m_entities (and mutate children lists), which would
        // invalidate an iterator/reference into it if we walked the
        // live list instead.
        const std::vector<EntityId> childrenCopy = m_entities.at(entity).children;
        for (const EntityId child : childrenCopy)
        {
            DestroyEntity(child);
        }

        // Detach from parent, if any, so the parent's children list
        // doesn't keep a reference to this now-destroyed entity.
        const EntityId parent = m_entities.at(entity).parent;
        if (parent.IsValid())
        {
            const auto it = m_entities.find(parent);
            if (it != m_entities.end())
            {
                auto& siblings = it->second.children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
            }
        }

        m_entities.erase(entity);
    }

    bool Scene::IsValid(EntityId entity) const
    {
        return entity.IsValid() && m_entities.contains(entity);
    }

    const std::string& Scene::GetName(EntityId entity) const
    {
        return GetRecord(entity).name;
    }

    Transform& Scene::GetTransform(EntityId entity)
    {
        return GetRecordMutable(entity).transform;
    }

    const Transform& Scene::GetTransform(EntityId entity) const
    {
        return GetRecord(entity).transform;
    }

    Core::Math::Mat4 Scene::GetWorldMatrix(EntityId entity) const
    {
        const EntityRecord& record = GetRecord(entity);

        Core::Math::Mat4 worldMatrix = record.transform.ToMatrix();
        EntityId current = record.parent;

        // Bounded walk, not unbounded: protects against malformed/
        // cyclic hierarchy data even though SetParent already prevents
        // cycles from being created in the first place. A well-formed
        // hierarchy with N entities has a parent chain no longer than
        // N, so N+1 steps is a safe bound that can never be reached
        // honestly.
        std::size_t stepsRemaining = m_entities.size() + 1;

        while (current.IsValid() && stepsRemaining > 0)
        {
            --stepsRemaining;

            const auto it = m_entities.find(current);
            if (it == m_entities.end())
            {
                break;
            }

            worldMatrix = it->second.transform.ToMatrix() * worldMatrix;
            current = it->second.parent;
        }

        AR_ASSERT_MSG(!current.IsValid() || stepsRemaining > 0,
                      "Scene hierarchy parent chain did not terminate - possible cycle in stored data");

        return worldMatrix;
    }

    bool Scene::SetParent(EntityId child, EntityId parent)
    {
        if (!IsValid(child) || !IsValid(parent))
        {
            return false;
        }

        if (child == parent)
        {
            return false; // an entity cannot parent itself
        }

        if (IsDescendantOf(parent, child))
        {
            // `parent` is already somewhere under `child` — parenting
            // `child` to it would create a cycle.
            return false;
        }

        EntityRecord& childRecord = m_entities.at(child);

        if (childRecord.parent.IsValid())
        {
            auto& oldSiblings = m_entities.at(childRecord.parent).children;
            oldSiblings.erase(std::remove(oldSiblings.begin(), oldSiblings.end(), child), oldSiblings.end());
        }

        childRecord.parent = parent;
        m_entities.at(parent).children.push_back(child);

        return true;
    }

    void Scene::ClearParent(EntityId child)
    {
        if (!IsValid(child))
        {
            return;
        }

        EntityRecord& childRecord = m_entities.at(child);
        if (!childRecord.parent.IsValid())
        {
            return; // already a root
        }

        const auto it = m_entities.find(childRecord.parent);
        if (it != m_entities.end())
        {
            auto& siblings = it->second.children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
        }

        childRecord.parent = EntityId{};
    }

    EntityId Scene::GetParent(EntityId entity) const
    {
        return GetRecord(entity).parent;
    }

    const std::vector<EntityId>& Scene::GetChildren(EntityId entity) const
    {
        return GetRecord(entity).children;
    }

    void Scene::SetRenderable(EntityId entity, Renderable renderable)
    {
        GetRecordMutable(entity).renderable = renderable;
    }

    void Scene::RemoveRenderable(EntityId entity)
    {
        if (!IsValid(entity))
        {
            return;
        }
        m_entities.at(entity).renderable.reset();
    }

    bool Scene::HasRenderable(EntityId entity) const
    {
        return GetRecord(entity).renderable.has_value();
    }

    const Renderable* Scene::GetRenderable(EntityId entity) const
    {
        const EntityRecord& record = GetRecord(entity);
        return record.renderable.has_value() ? &record.renderable.value() : nullptr;
    }

    std::vector<RenderableInstance> Scene::ExtractRenderables() const
    {
        std::vector<RenderableInstance> result;
        result.reserve(m_entities.size());

        for (const auto& [id, record] : m_entities)
        {
            if (!record.renderable.has_value() || !record.renderable->visible)
            {
                continue;
            }

            result.push_back(RenderableInstance{
                id, GetWorldMatrix(id), record.renderable->mesh, record.renderable->material, record.renderable->tint});
        }

        return result;
    }

    const Scene::EntityRecord& Scene::GetRecord(EntityId entity) const
    {
        AR_ASSERT_MSG(IsValid(entity), "Scene query called with an invalid or unknown EntityId - check IsValid() first");
        return m_entities.at(entity);
    }

    Scene::EntityRecord& Scene::GetRecordMutable(EntityId entity)
    {
        AR_ASSERT_MSG(IsValid(entity), "Scene query called with an invalid or unknown EntityId - check IsValid() first");
        return m_entities.at(entity);
    }

    bool Scene::IsDescendantOf(EntityId candidate, EntityId ancestor) const
    {
        std::vector<EntityId> stack{ancestor};
        std::size_t stepsRemaining = m_entities.size() + 1;

        while (!stack.empty() && stepsRemaining > 0)
        {
            --stepsRemaining;

            const EntityId current = stack.back();
            stack.pop_back();

            const auto it = m_entities.find(current);
            if (it == m_entities.end())
            {
                continue;
            }

            for (const EntityId childOfCurrent : it->second.children)
            {
                if (childOfCurrent == candidate)
                {
                    return true;
                }
                stack.push_back(childOfCurrent);
            }
        }

        AR_ASSERT_MSG(stepsRemaining > 0, "Scene hierarchy traversal exceeded entity count - possible cycle in stored data");
        return false;
    }
}

#pragma once

#include "AREngine/Scene/EntityId.hpp"
#include "AREngine/Scene/Transform.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace AREngine::Scene
{
    // Owns a small 3D world: entities, their local transforms, and a
    // parent/child hierarchy. Represents world DATA only — Scene never
    // performs GPU rendering and has no idea Rendering exists. See
    // docs/ARCHITECTURE.md, "Why Scene Does Not Depend On Rendering".
    //
    // Not an ECS: no components, no archetypes, no reflection — just
    // entity identity + transform + hierarchy, which is what M5 needs
    // to prove. See docs/ARCHITECTURE.md, "What's Deferred to a Future
    // ECS/Component System".
    //
    // Deliberately not a singleton: an application owns Scene
    // instance(s) explicitly.
    //
    // Two different philosophies for invalid input, matching the
    // precedent set by Rendering::NullRenderDevice: commands that
    // mutate the scene using an EntityId that could plausibly be stale
    // (SetParent, ClearParent, DestroyEntity) reject predictably rather
    // than crash — a stale reference to a since-destroyed entity is
    // normal in real use. Queries that must return real data
    // (GetTransform, GetName, GetParent, GetChildren, GetWorldMatrix)
    // assert on an invalid/unknown EntityId instead, since there is no
    // safe fallback value that wouldn't risk silently masking a bug —
    // call IsValid() first if unsure. See docs/ARCHITECTURE.md.
    class Scene
    {
    public:
        [[nodiscard]] EntityId CreateEntity(std::string name = {});

        // Destroys `entity` and, per the documented rule, all of its
        // descendants (so no child is ever left referencing a destroyed
        // parent). A no-op if `entity` is invalid or unknown.
        void DestroyEntity(EntityId entity);

        [[nodiscard]] bool IsValid(EntityId entity) const;

        [[nodiscard]] const std::string& GetName(EntityId entity) const;

        [[nodiscard]] Transform& GetTransform(EntityId entity);
        [[nodiscard]] const Transform& GetTransform(EntityId entity) const;

        // Computes entity's final transform by walking up the parent
        // chain and composing each ancestor's local matrix. Not
        // cached — recomputed on every call. See docs/ARCHITECTURE.md,
        // "World Matrix".
        [[nodiscard]] Core::Math::Mat4 GetWorldMatrix(EntityId entity) const;

        // Sets `child`'s parent to `parent`. `child`'s LOCAL transform
        // is left unchanged (so its WORLD transform may change as a
        // result) — see docs/ARCHITECTURE.md, "Reparenting Behavior".
        // Returns false and makes no change if either id is invalid or
        // unknown, if child == parent, or if `parent` is currently a
        // descendant of `child` (which would create a cycle).
        bool SetParent(EntityId child, EntityId parent);

        // Removes child's parent, if any; child becomes a root. A
        // no-op if `child` is invalid, unknown, or already a root.
        // child's LOCAL transform is unchanged.
        void ClearParent(EntityId child);

        [[nodiscard]] EntityId GetParent(EntityId entity) const;

        [[nodiscard]] const std::vector<EntityId>& GetChildren(EntityId entity) const;

    private:
        struct EntityRecord
        {
            std::string name;
            Transform transform;
            EntityId parent; // invalid (id == 0) means "no parent, this is a root"
            std::vector<EntityId> children;
        };

        [[nodiscard]] const EntityRecord& GetRecord(EntityId entity) const;
        [[nodiscard]] EntityRecord& GetRecordMutable(EntityId entity);

        // Is `candidate` found somewhere in `ancestor`'s subtree
        // (children, grandchildren, ...)? Used by SetParent to reject
        // reparenting that would create a cycle. Iterative and bounded
        // by entity count, not recursive, so malformed/cyclic data
        // can't cause an infinite loop here either — defense in depth,
        // since SetParent itself is what's supposed to prevent cycles
        // from ever being created.
        [[nodiscard]] bool IsDescendantOf(EntityId candidate, EntityId ancestor) const;

        std::unordered_map<EntityId, EntityRecord> m_entities;
        std::uint64_t m_nextEntityId = 1;
    };
}

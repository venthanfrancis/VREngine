#pragma once

#include "AREngine/Scene/EntityId.hpp"
#include "AREngine/Scene/Renderable.hpp"
#include "AREngine/Scene/RenderableInstance.hpp"
#include "AREngine/Scene/Transform.hpp"

#include <optional>
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

        // M12: the minimal "this entity can be drawn" component. Follows
        // the same invalid-input philosophy as the rest of Scene's API —
        // SetRenderable/HasRenderable/GetRenderable assert on an
        // invalid/unknown EntityId (mirroring GetTransform: there is no
        // safe fallback that wouldn't risk masking a bug), while
        // RemoveRenderable no-ops on one (mirroring ClearParent: removing
        // a component from a possibly-already-destroyed entity is a
        // normal, plausible cross-frame pattern). GetRenderable returning
        // nullptr means "valid entity, no renderable set" — never
        // "invalid entity" — those two cases are deliberately not
        // conflated. See docs/ARCHITECTURE.md, "M12 - Renderable Scene
        // Integration Foundation".
        void SetRenderable(EntityId entity, Renderable renderable);
        void RemoveRenderable(EntityId entity);
        [[nodiscard]] bool HasRenderable(EntityId entity) const;
        [[nodiscard]] const Renderable* GetRenderable(EntityId entity) const;

        // Snapshots every entity that currently has a visible Renderable
        // into a flat, backend-neutral list, resolving each one's WORLD
        // transform via GetWorldMatrix (so hierarchy is already baked
        // in). Not cached — recomputed on every call, same philosophy as
        // GetWorldMatrix itself. Takes no view/camera parameter: the
        // result is meant to be rendered against any number of views
        // afterward, not extracted separately per view. See
        // docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration
        // Foundation".
        [[nodiscard]] std::vector<RenderableInstance> ExtractRenderables() const;

    private:
        struct EntityRecord
        {
            std::string name;
            Transform transform;
            EntityId parent; // invalid (id == 0) means "no parent, this is a root"
            std::vector<EntityId> children;
            std::optional<Renderable> renderable;
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

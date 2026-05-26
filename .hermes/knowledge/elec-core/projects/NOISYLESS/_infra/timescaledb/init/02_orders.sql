-- Orders table (Stripe checkout → commande)
CREATE TABLE IF NOT EXISTS noisyless.orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    stripe_session_id TEXT UNIQUE,
    email TEXT NOT NULL,
    name TEXT NOT NULL,
    amount INTEGER NOT NULL,  -- montant en centimes
    currency TEXT DEFAULT 'eur',
    status TEXT CHECK (status IN ('pending', 'paid', 'shipped', 'cancelled', 'refunded')) DEFAULT 'pending',
    stripe_payment_intent TEXT,
    shipping_address JSONB,
    metadata JSONB,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Order items (produits achetés dans une commande)
CREATE TABLE IF NOT EXISTS noisyless.order_items (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    order_id UUID NOT NULL REFERENCES noisyless.orders(id) ON DELETE CASCADE,
    product_id TEXT NOT NULL,
    product_name TEXT,
    quantity INTEGER NOT NULL DEFAULT 1,
    unit_amount INTEGER NOT NULL,  -- prix unitaire en centimes
    metadata JSONB,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Index pour lookup rapide par session Stripe
CREATE INDEX IF NOT EXISTS idx_orders_stripe_session ON noisyless.orders(stripe_session_id);

-- Index pour lookup par email
CREATE INDEX IF NOT EXISTS idx_orders_email ON noisyless.orders(email);

-- Index pour order_items par order_id
CREATE INDEX IF NOT EXISTS idx_order_items_order_id ON noisyless.order_items(order_id);
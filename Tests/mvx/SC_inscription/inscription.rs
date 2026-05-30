#![no_std]

multiversx_sc::imports!();

#[multiversx_sc::contract]
pub trait GameRegistration {

    /// Appelé uniquement lors du PREMIER déploiement
    #[init]
    fn init(&self, gain_addr: ManagedAddress, treasury_addr: ManagedAddress) {
        self.gain_address().set(&gain_addr);
        self.treasury_address().set(&treasury_addr);
        self.current_round().set(1u32);
    }

    /// Appelé lors de chaque MISE À JOUR (upgrade) du contrat
    /// Permet de modifier les adresses de destination si nécessaire
    #[upgrade]
    fn upgrade(&self, gain_addr: ManagedAddress, treasury_addr: ManagedAddress) {
        self.gain_address().set(&gain_addr);
        self.treasury_address().set(&treasury_addr);
        // On ne réinitialise pas le round ici pour ne pas casser la partie en cours,
        // sauf si c'est ton souhait explicite.
    }

    // --- Fonctions Publiques (Endpoints) ---

    #[payable("EGLD")]
    #[endpoint(register)]
    fn register(&self) {
        let payment_amount = self.call_value().egld().clone_value(); // On extrait la valeur réelle
        let required_amount = BigUint::from(1_000_000_000_000_000_000u64);

        require!(
            payment_amount == required_amount,
            "L'inscription coûte exactement 1 EGLD"
        );
        let caller = self.blockchain().get_caller();
        let current_round = self.current_round().get();
        
        require!(
            self.player_round_registered(&caller).get() != current_round,
            "Déjà inscrit pour ce round"
        );
        // Répartition 75% / 25%
        let gain_share = &payment_amount * 75u32 / 100u32;
        let treasury_share = &payment_amount - &gain_share;

        self.send().direct_egld(&self.gain_address().get(), &gain_share);
        self.send().direct_egld(&self.treasury_address().get(), &treasury_share);

        self.player_round_registered(&caller).set(current_round);
    }

    #[endpoint(endGame)]
    fn end_game(&self) {
        self.blockchain().check_caller_is_owner();
        self.current_round().update(|val| *val += 1);
    }

    // --- Vues ---

    #[view(isRegistered)]
    fn is_registered(&self, address: ManagedAddress) -> bool {
        self.player_round_registered(&address).get() == self.current_round().get()
    }

    // --- Stockage ---

    #[view(getCurrentRound)]
    #[storage_mapper("current_round")]
    fn current_round(&self) -> SingleValueMapper<u32>;

    #[storage_mapper("player_round_registered")]
    fn player_round_registered(&self, address: &ManagedAddress) -> SingleValueMapper<u32>;

    #[storage_mapper("gain_address")]
    fn gain_address(&self) -> SingleValueMapper<ManagedAddress>;

    #[storage_mapper("treasury_address")]
    fn treasury_address(&self) -> SingleValueMapper<ManagedAddress>;
}

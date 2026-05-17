#![no_std]

multiversx_sc::imports!();

#[multiversx_sc::contract]
pub trait GainsContract {

    #[init]
    fn init(&self, admin_address: ManagedAddress, inscription_sc: ManagedAddress) {
        self.admin_address().set(admin_address);
        self.inscription_contract().set(inscription_sc);
        self.is_paused().set(false);
    }
    #[upgrade]
    fn upgrade(&self, admin_address: ManagedAddress, inscription_sc: ManagedAddress) {
        // Tu peux choisir de mettre à jour les adresses ou de les laisser
        // Ici, on permet de redéfinir l'admin et le contrat d'inscription lors de l'upgrade
        self.admin_address().set(admin_address);
        self.inscription_contract().set(inscription_sc);
        
        // On s'assure que le contrat n'est pas bloqué en pause par erreur après l'upgrade
        // (Optionnel, selon ton besoin)
        self.is_paused().set(false);
    }

    // --- LOGIQUE ADMIN (DESIGNATION) ---

    #[endpoint]
    fn set_winner(&self, winner_address: ManagedAddress) {
        require!(!self.is_paused().get(), "Contrat en pause");
        
        let caller = self.blockchain().get_caller();
        require!(
            caller == self.admin_address().get(),
            "Seul l'admin peut designer le gagnant"
        );

        // Extraction de la valeur brute u64 du timestamp actuel
        let current_time_raw = self.blockchain().get_block_timestamp_seconds().as_u64_seconds();
        let delay_seconds = 86_400u64; // 24 heures
        let release_time = current_time_raw + delay_seconds;

        self.pending_withdrawal_address().set(&winner_address);
        self.release_timestamp().set(release_time);
    }

    // --- LOGIQUE GAGNANT (PULL) ---

    #[endpoint]
    fn claim_gains(&self) {
        require!(!self.is_paused().get(), "Contrat en pause");
        
        let caller = self.blockchain().get_caller();
        require!(!self.pending_withdrawal_address().is_empty(), "Aucun gagnant designe");
        
        let winner = self.pending_withdrawal_address().get();
        require!(caller == winner, "Seul le gagnant designe peut reclamer");

        // Comparaison en u64 brut
        let current_time_raw = self.blockchain().get_block_timestamp_seconds().as_u64_seconds();
        let release_time = self.release_timestamp().get();
        
        require!(
            current_time_raw >= release_time, 
            "Periode de verrouillage de 24h non ecoulee"
        );

        let balance = self.blockchain().get_sc_balance(&EgldOrEsdtTokenIdentifier::egld(), 0);
        require!(balance > 0, "Aucun gain disponible");

        // Nettoyage avant l'envoi (Protection anti-réentrance)
        self.pending_withdrawal_address().clear();
        self.release_timestamp().clear();

        self.send().direct_egld(&winner, &balance);
    }

    // --- FONCTION DE DEPOT ---


    #[endpoint]
    #[payable("EGLD")]
    fn deposit(&self) {
        // Cet endpoint permet de recevoir des fonds via un appel explicite à "deposit"
    }

    #[endpoint]
    #[payable("EGLD")]
    fn receive(&self) {
        // Cet endpoint spécial permet de recevoir des fonds lors d'un transfert direct 
        // (sans nom de fonction spécifié) depuis le contrat d'Inscription.
    }

    // --- SECURITE PROPRIETAIRE ---

    #[endpoint]
    fn toggle_pause(&self) {
        let caller = self.blockchain().get_caller();
        let owner = self.blockchain().get_owner_address();
        require!(caller == owner, "Seul le proprietaire peut pauser le contrat");

        let current_state = self.is_paused().get();
        self.is_paused().set(!current_state);
    }

    // --- STORAGE MAPPERS ---

    #[view(getAdminAddress)]
    #[storage_mapper("admin_address")]
    fn admin_address(&self) -> SingleValueMapper<ManagedAddress>;

    #[view(getInscriptionContract)]
    #[storage_mapper("inscription_contract")]
    fn inscription_contract(&self) -> SingleValueMapper<ManagedAddress>;

    #[view(getPendingWinner)]
    #[storage_mapper("pending_withdrawal_address")]
    fn pending_withdrawal_address(&self) -> SingleValueMapper<ManagedAddress>;

    #[view(getReleaseTimestamp)]
    #[storage_mapper("release_timestamp")]
    fn release_timestamp(&self) -> SingleValueMapper<u64>;

    #[view(isPaused)]
    #[storage_mapper("is_paused")]
    fn is_paused(&self) -> SingleValueMapper<bool>;
}
